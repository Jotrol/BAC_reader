#pragma once

#include "Util.hpp"

#include <map>
#include <vector>
#include <fstream>
using namespace std;

namespace BerTLV {
	typedef basic_fstream<unsigned char> BerStream;

	/* ������ ���� */
	enum class BerTagClass { UNIVERSAL, APPLICATION, CONTEXT_SPECIFIC, PRIVATE };

	/* ��� ���� */
	enum class BerTagType { PRIMITIVE, CONSTRUCTED };

	/* ����� ������ ������ BER-TLV ���� */
	class BerTLVDecoderToken {
	private:
		/* ���� typedef'��, ����� ���� ������� */
		typedef tuple<BerTagClass, BerTagType, UINT64> BerTagTuple;
		typedef tuple<UINT32, UINT32> BerLenTuple;

		/* ��� ������ */
		UINT64 berTag;

		/* ����� ���� */
		UINT32 berLen;

		/* �������� ������ ���� ������ ������ */
		UINT32 berDataStart;

		/* ����� ���� */
		BerTagClass berTagClass;

		/* ��� ���� */
		BerTagType berTagType;

		/* ������ �������� ���� */
		INT16 berParentIndex;

		/* ������� ��������� ���� */
		BerTagTuple decodeTag(BerStream& stream) {
			/* ������� �������� ���� ���� */
			UINT8 firstByte = stream.get();

			/* ���������� ����� � ��� ���� */
			BerTagClass berTagClassRaw = (BerTagClass)((firstByte >> 6) & 0x03);
			BerTagType berTagTypeRaw = (BerTagType)((firstByte >> 5) & 0x01);

			/* �������� ��� ����� ���� */
			UINT64 berTagRaw = firstByte;

			/* ���� 5 ��� ���� �� ����� �������� - �� ��� ������������ ��� */
			if ((firstByte & 0x1F) == 0x1F) {
				/* �����, ���� �������� ������ ����� ���� */
				for (UINT i = 0; i < 8; i += 1) {
					/* �������� ��������� ���� ���� */
					UINT8 tagNum = stream.get();

					/* ���������� ��� � �������� ����� ���� */
					berTagRaw = (berTagRaw << 8) | tagNum;

					/* �������, ���������� �� ������� ���, ���� ��� - ��� ����� ������ ���� */
					if (((tagNum >> 7) & 0x01) == 0) { i = 8; }
				}
			}

			/* ���������� ������ � ������������ ������������ ������ - tuple */
			return make_tuple(berTagClassRaw, berTagTypeRaw, berTagRaw);
		}

		/* ������� ���������  */
		BerLenTuple decodeLength(BerStream& stream) {
			/* ��������� ������ ���� ����� */
			UINT8 firstByte = stream.get();

			/* ���� � ������ ����� ����� ���������� ������� ��� */
			/* �� ����� ������������ � ������ ������ */
			/* ��� - �� ����� ������� �����(��� �������� ����) */
			UINT32 berLenRaw = firstByte;
			if ((firstByte & 0x80) == 0x80) {
				/* �������� ���-�� ������ ����� */
				UINT8 lenBytes = (firstByte & 0x7F);

				/* ����� ����������� ����� ����� ������ 4 ����� ������ */
				if (lenBytes >= sizeof(berLen)) {
					throw std::exception("������: ����� ������ ������, ��� ���, ��������� � ��������");
				}

				/* ��������� ��� �����, ���� �� ����� �� ���� */
				berLenRaw = 0;
				for (UINT8 i = 0; i < lenBytes; i += 1) {
					berLenRaw = (berLenRaw << 8) | (UINT8)stream.get();
				}
			}

			/* ���������� ������ ����� ������ ���� */
			return make_tuple((UINT32)stream.tellg(), berLenRaw);
		}

	public:
		BerTLVDecoderToken() = default;

		BerTLVDecoderToken(BerStream& stream, INT16 parentIdx) {
			/* ���������� ��� */
			BerTagTuple tagTuple = decodeTag(stream);

			/* ���������� ����� ������ */
			BerLenTuple lenTuple = decodeLength(stream);

			/* ����������� ��� ���������� */
			berTagClass = get<0>(tagTuple);
			berTagType = get<1>(tagTuple);
			berTag = get<2>(tagTuple);

			berDataStart = get<0>(lenTuple);
			berLen = get<1>(lenTuple);

			berParentIndex = parentIdx;

			/* ���� ��� �� ��������� ���, �� ��� ������ ����� ���������� */
			if (berTagType == BerTagType::PRIMITIVE) {
				stream.seekg(berLen, ios::cur);
			}
		}

		/* �������� �� ��, ��� ��� ��������� */
		bool isConstructed() { return berTagType == BerTagType::CONSTRUCTED; };

		/* �������� ��� */
		UINT64 getTag() { return berTag; }

		/* �������� ������ ������ */
		UINT32 getDataStart() { return berDataStart; }

		/* �������� ����� ������ */
		UINT32 getDataLen() { return berLen; }

		/* �������� �������� ���� */
		UINT16 getParent() { return berParentIndex; }

		/* � ������, ����� ������� ����� �������� ������������� ����� */
		/* ����� �������� ������ ������� */
	};

	/* ����� ������������� BER-TLV ������ */
	class BerTLVDecoder {
	private:
		/* ������� �������������, ������� ���������� ���� ���������� */
		UINT16 decode(BerStream& stream, INT16 parentIndex, UINT32 parentTokenEnd) {
			/* ������� ����� ������, ������� �����, ����� ����� ������������ */
			while (stream.tellg() < parentTokenEnd) {
				/* ������� ����� ����� */
				BerTLVDecoderToken token(stream, parentIndex);

				/* �������� � ������ ������� */
				tokens[tokenSize++] = token;

				/* ���������, �� ����������� �� ����� ��� ������� */
				checkForExtend();

				/* ���� ����� ��������� */
				if (token.isConstructed()) {
					/* ������ ������������ ������, ��������� */
					/* ����� ����� � ������ �������� ������ ������� �������� ������ */
					decode(stream, tokenSize - 1, token.getDataLen() + (UINT32)stream.tellg());
				}
			}

			/* ������� ���� � �������� ������� - ��� ��������� �� ������ ������� ������� */
			return 0;
		}

		/* ������� �������� ����������� ����� � ������� ������� */
		void checkForExtend() {
			/* ���� ������ ������� �� ��������� ������������� ������� */
			if (tokenSize < tokenMaxSize) {
				/* �� ������ ������ �� ���� */
				return;
			}

			/* ����� ����� �������� ������������ ����������� */
			tokenMaxSize *= 2;

			/* ������� ����� ������ � ����� ������������ */
			auto newPiece = make_unique<BerTLVDecoderToken[]>(tokenMaxSize);

			/* ��������� ������ ����� ������ � ����� */
			memcpy(newPiece.get(), tokens.get(), tokenSize * sizeof(BerTLVDecoderToken));

			/* ��������� ������ ������ */
			tokens.release();

			/* �������� ��������� - ������ newPiece = nullptr */
			/* � tokens - ������ � ���� ����� ������ ������� */
			tokens.swap(newPiece);
		}
	public:
		/* ������������ ������ ������� ������� */
		UINT16 tokenMaxSize = 8;

		/* ������� ������ ������� ������� */
		UINT16 tokenSize = 0;

		/* ��� ������ ������� */
		unique_ptr<BerTLVDecoderToken[]> tokens;

		/* ����������� �������� */
		BerTLVDecoder() : tokens(new BerTLVDecoderToken[tokenMaxSize]) {}

		/* ������� ������������� */
		UINT16 decode(BerStream& stream) {
			/* �������� ������ ������ ������ */
			stream.seekg(0, ios::end);
			UINT32 streamLen = (UINT32)stream.tellg();
			stream.seekg(0);

			/* �������� ������� ������ ������� */
			tokenSize = 0;
			return decode(stream, 0, streamLen);
		}

		/* ������ �������� ������ ������ �� ��� ���� ������ ���������� ���� */
		INT16 getChildByTag(INT16 parent, UINT64 childTag) {
			/* �������� � ������� ���� ��� ��������� (��� ��� � ������� ��� ��������� ������) */
			for (UINT16 i = parent; i < tokenSize; i += 1) {
				/* �������� ����� �� ������� */
				BerTLVDecoderToken* token = &tokens[i];

				/* � ��������� ��� ������������ */
				if (token->getTag() == childTag && token->getParent() == parent) {
					/* ���������� ������ ����� ������, ���� ��� ���, ��� ����� */
					return i;
				}
			}

			/* �����, ���� �� ������ �����, ������� ���� */
			return -1;
		}

		/* ��������� ������ ������ (��� ���������, ���� ������) */
		BerTLVDecoderToken* getTokenPtr(UINT16 tokenIndex) {
			/* ���� �����-�� ������� ������ ������ �������� ������� ������� */
			if (tokenIndex > tokenSize) {
				/* �� ������� ������� ��������� */
				return nullptr;
			}

			/* �� ���� ��������� �������, ������� ����� ������ �� ������� */
			return &tokens[tokenIndex];
		}

		vector<UINT8> getTokenRaw(BerStream& file, UINT16 tokenIndex) {
			/* ���� �����-�� ������� ������ ������ �������� ������� ������� */
			if (tokenIndex > tokenSize) {
				/* �� ������� ������� ��������� */
				return {};
			}

			UINT32 tokenRawStart = 0;
			if (tokenIndex > 0) {
				UINT16 prevIndex = tokenIndex - 1;
				tokenRawStart = tokens[prevIndex].getDataStart() + tokens[prevIndex].getDataLen();
			}

			UINT32 tokenRawEnd = tokens[tokenIndex].getDataStart() + tokens[tokenIndex].getDataLen();
			UINT32 tokenRawLen = tokenRawEnd - tokenRawStart;

			vector<UINT8> rawToken(tokenRawLen, 0);
			file.seekg(tokenRawStart);
			file.read(rawToken.data(), tokenRawLen);
			return rawToken;
		}

		/* ������� ��� ��������� ������ ���� */
		vector<UINT8> getTokenData(BerStream& stream, UINT16 tokenIndex) {
			BerTLVDecoderToken* token = &tokens[tokenIndex];
			stream.seekg(token->getDataStart());

			UINT32 tokenDataLen = token->getDataLen();
			vector<UINT8> data(tokenDataLen, 0);
			stream.read(data.data(), tokenDataLen);
			return data;
		}
	};

	class BerTLVCoderToken {
	private:
		BerTagType berTagType;

		vector<BerTLVCoderToken> childTokens;
		vector<UINT8> berData;
		vector<UINT8> berTag;

		UINT8 getFirstNonZeroByteIndex(UINT8* arr, UINT8 arrLen) const {
			UINT8 nonZeroIndex = 0;
			for (; nonZeroIndex < arrLen && !arr[nonZeroIndex]; nonZeroIndex += 1);
			return nonZeroIndex;
		}

		vector<UINT8> encodeLen(UINT64 berLen) const {
			if (berLen < 0x80ULL) {
				return { (UINT8)(berLen & 0xFF) };
			}
			
			UINT8 lenRaw[sizeof(UINT64)] = { 0 };
			UINT64 temp = Util::changeEndiannes(berLen);
			memcpy(lenRaw, &temp, sizeof(UINT64));
			
			UINT8 nonZero = getFirstNonZeroByteIndex(lenRaw, sizeof(UINT64));
			UINT8 lenSize = sizeof(UINT64) - nonZero;
			vector<UINT8> lenEnc = { lenSize };
			for (UINT i = 0; i < sizeof(UINT64); i += 1) {
				lenEnc.push_back(lenRaw[i]);
			}
			return lenEnc;
		}
		vector<UINT8> encodeValue() const {
			if (berTagType == BerTagType::PRIMITIVE) {
				return berData;
			}

			vector<UINT8> value;
			for (auto& child : childTokens) {
				vector<UINT8> childEnc = child.encode();
				value.insert(value.end(), childEnc.begin(), childEnc.end());
			}

			return value;
		}
	public:
		BerTLVCoderToken(UINT64 newBerTag) : childTokens(), berData() {
			/* ���������� ������� ����, ������� �� ����� ���� */
			/* ��� ��� �������� ������������ ������ �� bigendian */
			/* ������, ����� ������� - ���������, � ������� - ������ */
			/* ������ ���� ������� �����. ����� ��� � ���� �������� ������ ��������� */
			UINT8 berTagRaw[sizeof(UINT64)] = { 0 };
			newBerTag = Util::changeEndiannes(newBerTag);
			memcpy(berTagRaw, &newBerTag, sizeof(UINT64));

			UINT8 nonZero = getFirstNonZeroByteIndex(berTagRaw, sizeof(UINT64));

			/* ����� ����� ����, ���� ��� � �������� ����� ���� */
			UINT8 firstByte = berTagRaw[nonZero];
			berTagType = (BerTagType)((firstByte >> 5) & 0x01);

			/* �������� ����� ���� � ������ � bigendian */
			for (INT i = nonZero; i < sizeof(UINT64); i += 1) {
				berTag.push_back(berTagRaw[i]);
			}
		}
		
		void addData(const UINT8* data, UINT16 dataLen) {
			for (int i = 0; i < dataLen; i += 1) {
				berData.push_back(data[i]);
			}
		}
		void addData(const vector<UINT8>& data) {
			berData.insert(berData.end(), data.begin(), data.end());
		}
		void addChild(BerTLVCoderToken& child) {
			childTokens.push_back(child);
		}

		vector<UINT8> encode() const {
			/* �������� ������ ���� */
			vector<UINT8> valEnc = encodeValue();

			/* ���� ������ ��� - �� ��� � ����, �������? */
			if (valEnc.size() == 0) {
				return {};
			}

			vector<UINT8> lenEnc = encodeLen(valEnc.size());

			vector<UINT8> result;
			result.insert(result.end(), berTag.begin(), berTag.end());
			result.insert(result.end(), lenEnc.begin(), lenEnc.end());
			result.insert(result.end(), valEnc.begin(), valEnc.end());

			return result;
		}

		UINT64 getDataLen() const {
			return berData.size();
		}
	};
}