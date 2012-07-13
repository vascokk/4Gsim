//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

/* See 3GPP TS 24007 chapter 11.2 */

#include "NASSerializer.h"
#include "NASUtils.h"
#include <platdep/sockets.h>

NASSerializer::NASSerializer() {
	// TODO Auto-generated constructor stub
	shift = 0;
}

NASSerializer::~NASSerializer() {
	// TODO Auto-generated destructor stub
}

unsigned NASSerializer::calcLength(NASPlainMessage *msg) {
	unsigned len = msg->getHdr().getMsgType() & 0x80 ? NAS_ESM_HEADER_SIZE : NAS_EMM_HEADER_SIZE;
	for (unsigned i = 0; i < msg->getIesArraySize(); i++) {
		NASInfoElem ie = msg->getIes(i);
		switch(ie.getIeType()) {
			case IEType1:
				if (ie.getFormat() == IE_TV)
					len++;
				else if (ie.getFormat() == IE_V) {
					len += (shift + 1) % 2;
					shift = (shift + 1) % 2;
				}
				break;
			case IEType2:
				len++;
				break;
			case IEType3:
				len += ie.getValueArraySize();
				if (ie.getFormat() == IE_TV)
					len++;
				break;
			case IEType4:
				len += ie.getValueArraySize() + 1;
				if (ie.getFormat() == IE_TLV)
					len++;
				break;
			case IEType6:
				len += ie.getValueArraySize() + 2;
				if (ie.getFormat() == IE_TLVE)
					len++;
				break;
			default:;
		}
	}
	return len;
}

unsigned NASSerializer::serializeHeader(NASHeader hdr, char *p) {

	if (hdr.getMsgType() & 0x80) {	// is ESM message
		*((unsigned char*)(p++)) = (hdr.getEpsBearerId() << 4) | (hdr.getProtDiscr());
		*((unsigned char*)p++) = hdr.getProcTransId();
	} else	{						// for EMM message
		*((unsigned char*)(p++)) = (hdr.getSecHdrType() << 4) | (hdr.getProtDiscr());
	}
	*((unsigned char*)p++) = hdr.getMsgType();
	return hdr.getLength();
}

NASHeader NASSerializer::parseHeader(char *p) {
	NASHeader hdr = NASHeader();
	hdr.setProtDiscr(*((unsigned char*)(p)) & 0x0f);
	if (hdr.getProtDiscr() == ESMMessage) {	// is ESM message
		hdr.setEpsBearerId((*((unsigned char*)(p++)) >> 4) & 0x0f);
		hdr.setLength(NAS_ESM_HEADER_SIZE);
		hdr.setProcTransId(*((unsigned char*)p++));
	} else if (hdr.getProtDiscr() == EMMMessage) {	// for EMM message
		hdr.setSecHdrType((*((unsigned char*)(p++)) >> 4) & 0x0f);
		hdr.setLength(NAS_EMM_HEADER_SIZE);
	}
	hdr.setMsgType(*((unsigned char*)p++));
	return hdr;
}

unsigned NASSerializer::serializeIE(NASInfoElem ie, char *p) {
	char *begin = p;
	switch(ie.getIeType()) {
		case IEType1:
			if (ie.getFormat() == IE_TV) {
				*((unsigned char*)(p++)) = (ie.getType() << 4) | (ie.getValue(0) & 0x0f);
			} else if (ie.getFormat() == IE_V) { // shifting only for type 1 (after one IE type 1 follows another with type 1)
				*((unsigned char*)(p)) += ((unsigned char)ie.getValue(0)) << (((shift + 1) % 2) * 4);
				p += shift % 2;
				shift = (shift + 1) % 2;
			}
			break;
		case IEType2:
			*((unsigned char*)(p++)) = ie.getType();
			break;
		case IEType3:
			if (ie.getFormat() == IE_TV)
				*((unsigned char*)(p++)) = ie.getType();
			for (unsigned i = 0; i < ie.getValueArraySize(); i++)
				*((char*)(p++)) = ie.getValue(i);
			break;
		case IEType4:
			if (ie.getFormat() == IE_TLV)
				*((unsigned char*)(p++)) = ie.getType();
			*((unsigned char*)(p++)) = ie.getValueArraySize();
			for (unsigned i = 0; i < ie.getValueArraySize(); i++)
				*((char*)(p++)) = ie.getValue(i);
			break;
		case IEType6:
			if (ie.getFormat() == IE_TLVE)
				*((unsigned char*)(p++)) = ie.getType();
			*((unsigned short*)p) = ntohs(ie.getValueArraySize());
			p += 2;
			for (unsigned i = 0; i < ie.getValueArraySize(); i++)
				*((char*)(p++)) = ie.getValue(i);
			break;
		default:;
	}
	return p - begin;
}

NASInfoElem NASSerializer::parseIE(char *p, unsigned char format, unsigned char ieType, unsigned short length) {
	NASInfoElem ie = NASInfoElem();
	ie.setIeType(ieType);
	ie.setFormat(format);
	ie.setValueArraySize(length);
	char *begin = p;
	switch(ie.getIeType()) {
		case IEType1:
			ie.setValueArraySize(1);
			if (ie.getFormat() == IE_TV) {
				ie.setType((*((unsigned char*)(p)) >> 4) & 0x0f);
				ie.setValue(0, *((unsigned char*)(p++)) & 0x0f);
			} else if (ie.getFormat() == IE_V) { // shifting only for type 1 (after one IE type 1 follows another with type 1)
				ie.setValue(0, *((unsigned char*)(p)) >> (((shift + 1) % 2) * 4) & 0x0f);
				p += shift % 2;
				shift = (shift + 1) % 2;
			}
			break;
		case IEType2:
			ie.setType(*((unsigned char*)(p++)));
			break;
		case IEType3:
			if (ie.getFormat() == IE_TV)
				ie.setType(*((unsigned char*)(p++)));
			for (unsigned i = 0; i < ie.getValueArraySize(); i++)
				ie.setValue(i, *((char*)(p++)));
			break;
		case IEType4:
			if (ie.getFormat() == IE_TLV)
				ie.setType(*((unsigned char*)(p++)));
			ie.setValueArraySize(*((unsigned char*)(p++)));
			for (unsigned i = 0; i < ie.getValueArraySize(); i++)
				ie.setValue(i, *((char*)(p++)));
			break;
		case IEType6:
			if (ie.getFormat() == IE_TLVE)
				ie.setType(*((unsigned char*)(p++)));
			ie.setValueArraySize(ntohs(*((unsigned short*)p)));
			p += 2;
			for (unsigned i = 0; i < ie.getValueArraySize(); i++)
				ie.setValue(i, *((char*)(p++)));
			break;
		default:;
	}
	skip = p - begin;
	return ie;
}

unsigned NASSerializer::serialize(NASPlainMessage *msg, char *&buffer) {
	NASPlainMessage *encMsg = NULL;
	if (msg->getEncapsulatedPacket() != NULL)
		encMsg = check_and_cast<NASPlainMessage*>(msg->decapsulate());
	unsigned len = calcLength(msg);
	int encPos = -1;
	if (encMsg != NULL) {
		len += calcLength(encMsg) + 2;
		encPos = msg->getEncapPos();
	}
	buffer = (char*)calloc(len, sizeof(char));
	char *p = buffer;

	p += serializeHeader(msg->getHdr(), p);
	for (unsigned i = 0; i < msg->getIesArraySize(); i++) {
		if ((i == (unsigned)encPos) && (encPos != -1)) {
			char *esmCont;
			unsigned esmContLen = NASSerializer().serialize(encMsg, esmCont);
			NASInfoElem ie = NASUtils().createIE(IE_LVE, IEType6, 0, esmContLen, esmCont);
			p += serializeIE(ie, p);
		}
		p += serializeIE(msg->getIes(i), p);
	}
	delete encMsg;
	return len;
}

//NASPlainMessage *NASSerializer::parse(char *buffer, unsigned len) {
//	NASPlainMessage *msg = new NASPlainMessage();
//	char *p = buffer;
//	NASHeader hdr = parseHeader(p);
//	if (!hdr.getLength())
//		return NULL;
//	p += hdr.getLength();
//	msg->setHdr(hdr);
//	switch(hdr.getMsgType()) {
//		case AttachRequest:
//			msg->setIesArraySize(5);
//			msg->setName("Attach-Request");
//			/* NAS key set identifier */
//			msg->setIes(0, NASUtils().createIE(IE_V, IEType1));
//
//			/* EPS attach type */
//			msg->setIes(1, NASUtils().createIE(IE_V, IEType1));
//
//			/* EPS mobile identity */
//			msg->setIes(2, NASUtils().createIE(IE_LV, IEType4));
//
//			/* UE network capability */
//			msg->setIes(3, NASUtils().createIE(IE_LV, IEType4));
//
//			/* ESM message container */
//			msg->setIes(4, NASUtils().createIE(IE_LVE, IEType6));
//			break;
//		case PDNConnectivityRequest:
//			break;
//		default:;
//	}
//	for (unsigned i = 0; i < msg->getIesArraySize(); i++) {
//		NASInfoElem ie = msg->getIes(i);
//		p += parseIE(&ie, p);
//	}
//	return msg;
//}