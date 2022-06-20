#pragma once
#include <optional>
#include <vector>

class BerTLV {
public:
	enum class TagClass {
		UNIVERSAL,
		APPLICATION,
		CONTEXT_SPECIFIC,
		PRIVATE
	};

	enum class TagType
	{
		PRIMITIVE,
		CONSTRUCTED
	};

	static BerTLV decodeBerTLV(const std::vector<unsigned char>& rawData) {
		size_t currentReadIndex = 0;
		return BerTLV(rawData, currentReadIndex);
	}

	size_t getLength() const {
		return berLen;
	}

	size_t getTag() const {
		return berTag;
	}

	const std::vector<unsigned char>& getValue() const {
		return berValue;
	}

	std::optional<BerTLV> findChild(size_t tag) const {
		for (auto& child : childrenTlvs) {
			if (child.berTag == tag) {
				return child;
			}
		}

		return std::nullopt;
	}

	/* ����� ���� ����������� ������� �-�� findAll(tag) */

	std::vector<BerTLV>& getChildren() {
		return childrenTlvs;
	}

private:
	size_t berTag;
	size_t berLen;
	std::vector<unsigned char> berValue;

	bool isValidTlv = true;
	bool hasMoreAfterVal = false;

	TagClass berTagClass = TagClass::UNIVERSAL;
	TagType berTagType = TagType::PRIMITIVE;
	std::vector<BerTLV> childrenTlvs;

	BerTLV(const std::vector<unsigned char>& rawData, size_t& currentReadIndex) {
		std::tuple<TagClass, TagType, size_t> berTagTuple = decodeTag(rawData, currentReadIndex);

		berTagClass = std::get<0>(berTagTuple);
		berTagType = std::get<1>(berTagTuple);
		berTag = std::get<2>(berTagTuple);

		berLen = decodeLength(rawData, currentReadIndex);
		if (berLen == 0) {
			throw "������-�� ����� BER-TLV ����� ����";
		}

		if (berTagType == TagType::CONSTRUCTED) {
			childrenTlvs = decodeChildrenTags(rawData, currentReadIndex);
		}
		else {
			auto start = rawData.begin() + currentReadIndex;
			berValue.resize(berLen);
			std::copy(start, start + berLen, berValue.begin());
			currentReadIndex += berLen;
		}
	}

	std::tuple<TagClass, TagType, size_t> decodeTag(const std::vector<unsigned char>& tlvRaw, size_t& currentReadIndex) {
		/* ��������� ����� ���� ������������������ */
		const unsigned char firstByte = tlvRaw[currentReadIndex];
		currentReadIndex += 1;

		/* �������� 2 ������� ���� - ��� ����� ���� */
		const int tagClassBits = (firstByte >> 6) & 0x03;

		/* �������� ��� ���� ����: ���� �� �������, ���� ��������� */
		const int tagTypeBit = (firstByte >> 5) & 0x01;


		/* �������� �������� ������ ���� � ������ ����� */
		size_t tagNumber = firstByte;

		/* ���������, �� ����� �� ��� ���� ������ ���� ����� 0x1F */
		if ((tagNumber & 0x1F) == 0x1F) {
			/* ��������� ��� ����� ���� (�� ������ ~ 8 ����, ����� �������� ��� ������������) */
			for (; currentReadIndex < tlvRaw.size();) {
				/* ������� � ������ ������ ����� ���� */
				/* ������� ��� ����� �� ����������� ��� �������� */
				unsigned char tagNum = tlvRaw[currentReadIndex] & 0xFF;
				currentReadIndex += 1;

				/* ������� ����� ��� ���������� ��� �� 8 ��� � ������� ���� � ����� */
				tagNumber = (tagNumber << 8) | tagNum;

				/* ���� ��� ��� ��������� ���� ���� - ��������� */
				if (((tagNum >> 7) & 0x1) == 0) {
					break;
				}
			}
		}

		/* �������� tagClassBits ����� � �������� ����� 0 � 3, ��� ��� �� ������� �� ������ */
		/* ����� ������ ������������ ����� if-��, ��� ����� ������� */
		/* �� �� ����� ����� ������� � � tagType */
		return std::make_tuple((TagClass)tagClassBits, (TagType)tagTypeBit, tagNumber);
	}

	size_t decodeLength(const std::vector<unsigned char>& tlvRaw, size_t& currentReadIndex) {
		/* �������� ����, ��� ������� �� ����� �� ������� �������� �������� ������ */
		if (currentReadIndex >= tlvRaw.size()) {
			return 0;
		}

		/* ������ ������ ���� ����� � ��������� � ����. ����� */
		const unsigned char firstByte = tlvRaw[currentReadIndex];
		currentReadIndex += 1;

		/* ������� ����� */
		size_t length = firstByte;
		if (((firstByte >> 7) & 0x01) == 1) {
			/* ��������� ���-�� ������, ������������ ����� */
			const unsigned char lenBytes = (firstByte & 0x7F);

			/* ��������� ��� ����� ����������� ��� ����� ����� */
			length = 0;
			for (int i = 0; i < lenBytes && currentReadIndex < tlvRaw.size(); i += 1, currentReadIndex += 1) {
				length = (length << 8) | tlvRaw[currentReadIndex];
			}
		}

		/* ����������� ���������� ����������� ����� */
		return length;
	}

	std::vector<BerTLV> decodeChildrenTags(const std::vector<unsigned char>& tlvRaw, size_t& currentReadIndex) {
		size_t currentReadIndexChildren = 0;
		std::vector<BerTLV> children;
		while (currentReadIndexChildren != berLen) {
			/* ����������, ����� ���������� ������ ����� �������������� ���� */
			size_t beforeDecode = currentReadIndex;

			/* ���������� ��� � ��������� ��� � ������ �������������� */
			BerTLV nextTag(tlvRaw, currentReadIndex);
			children.push_back(nextTag);

			/* ��������� ���������� ����������� ������ */
			currentReadIndexChildren += currentReadIndex - beforeDecode;
		}

		return children;
	}
};