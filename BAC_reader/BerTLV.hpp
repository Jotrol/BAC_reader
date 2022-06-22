#pragma once

#include <windef.h>
#include <iosfwd>
using namespace std;

namespace BerTLV {
	/* ����� ������ ������ BER-TLV ���� */
	class BerTLVDecoderToken {
	private:
		/* ������ ���� */
		enum class BerTagClass { UNIVERSAL, APPLICATION, CONTEXT_SPECIFIC, PRIVATE };

		/* ��� ���� */
		enum class BerTagType { PRIMITIVE, CONSTRUCTED };

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
		UINT16 berParentIndex;

		/* ������� ��������� ���� */
		BerTagTuple decodeTag(istream& stream) {
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
		BerLenTuple decodeLength(istream& stream) {
			/* ��������� ������ ���� ����� */
			UINT8 firstByte = stream.get();

			/* ���� � ������ ����� ����� ���������� ������� ��� */
			/* �� ����� ������������ � ������ ������ */
			/* ��� - �� ����� ������� �����(��� �������� ����) */
			UINT64 berLenRaw = firstByte;
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
			return make_tuple(stream.tellg(), berLenRaw);
		}

	public:
		BerTLVDecoderToken() = default;

		BerTLVDecoderToken(istream& stream, UINT16 parentIdx) {
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

		/* ������� ��� ��������� ������ ���� */
		unique_ptr<UINT8> getData(istream& stream) {
			stream.seekg(berDataStart);

			unique_ptr<UINT8> data(new UINT8[berLen]);
			stream.read((char*)data.get(), berLen);
			return data;
		}

		/* � ������, ����� ������� ����� �������� ������������� ����� */
		/* ����� �������� ������ ������� */
	};

	/* ����� ������������� BER-TLV ������ */
	class BerTLVDecoder {
	private:
		/* ������� �������������, ������� ���������� ���� ���������� */
		UINT16 decode(istream& stream, UINT16 parentIndex, UINT32 parentTokenEnd) {
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
					decode(stream, tokenSize - 1, token.getDataLen() + stream.tellg());
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
		UINT16 decode(istream& stream) {
			/* �������� ������ ������ ������ */
			stream.seekg(0, ios::end);
			UINT32 streamLen = stream.tellg();
			stream.seekg(0);

			/* �������� ������� ������ ������� */
			tokenSize = 0;
			return decode(stream, 0, streamLen);
		}

		/* ������ �������� ������ ������ �� ��� ���� ������ ���������� ���� */
		UINT16 getChildByTag(UINT16 parent, UINT64 childTag) {
			/* �������� � ������� ���� ��� ��������� (��� ��� � ������� ��� ��������� ������) */
			for (UINT16 i = parent + 1; i < tokenSize; i += 1) {
				/* �������� ����� �� ������� */
				BerTLVDecoderToken* token = &tokens[i];

				/* � ��������� ��� ������������ */
				if (token->getTag() == childTag && token->getParent() == parent) {
					/* ���������� ������ ����� ������, ���� ��� ���, ��� ����� */
					return i;
				}
			}
			
			/* �����, ���� �� ������ �����, ������� ���� */
			return 0;
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
	};

	/* TODO: ������� BER-TLV ����������, ��� - ���� �� �������� */
}