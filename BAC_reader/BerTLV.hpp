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

	/* ћожет быть понадобитс€ функци€ а-л€ findAll(tag) */

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
			throw "ѕочему-то длина BER-TLV равна нулю";
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
		/* —читываем перый байт последовательности */
		const unsigned char firstByte = tlvRaw[currentReadIndex];
		currentReadIndex += 1;

		/* ¬ыдел€ем 2 старших бита - это класс тега */
		const int tagClassBits = (firstByte >> 6) & 0x03;

		/* ѕолучаем бит типа тега: либо он простой, либо составной */
		const int tagTypeBit = (firstByte >> 5) & 0x01;


		/* ѕолучаем значение номера тега в первом байте */
		size_t tagNumber = firstByte;

		/* ѕровер€ем, не равны ли все биты номера тега числу 0x1F */
		if ((tagNumber & 0x1F) == 0x1F) {
			/* —читываем все байты тега (не больше ~ 8 байт, иначе числовой тип переполнитс€) */
			for (; currentReadIndex < tlvRaw.size();) {
				/* ƒобавим в список байтов новый байт */
				/* —тарший бит будет не учитыватьс€ при подсчЄте */
				unsigned char tagNum = tlvRaw[currentReadIndex] & 0xFF;
				currentReadIndex += 1;

				/* —местим влево уже записанный тег на 8 бит и допишем байт в конец */
				tagNumber = (tagNumber << 8) | tagNum;

				/* ≈сли это был последний байт тега - остановка */
				if (((tagNum >> 7) & 0x1) == 0) {
					break;
				}
			}
		}

		/* «начение tagClassBits будут в пределах между 0 и 3, так что за пределы не выйдем */
		/* «десь просто пропускаютс€ много if-ов, так будет быстрее */
		/* “о же самое можно сделать и с tagType */
		return std::make_tuple((TagClass)tagClassBits, (TagType)tagTypeBit, tagNumber);
	}

	size_t decodeLength(const std::vector<unsigned char>& tlvRaw, size_t& currentReadIndex) {
		/* ѕроверка того, что декодер не вышел за пределы размеров исходных данных */
		if (currentReadIndex >= tlvRaw.size()) {
			return 0;
		}

		/* „итаем первый байт длины и переходим к след. байту */
		const unsigned char firstByte = tlvRaw[currentReadIndex];
		currentReadIndex += 1;

		/* —читаем длину */
		size_t length = firstByte;
		if (((firstByte >> 7) & 0x01) == 1) {
			/* ќпределим кол-во байтов, кодировавших длину */
			const unsigned char lenBytes = (firstByte & 0x7F);

			/* —читываем все байты необходимые дл€ длины байты */
			length = 0;
			for (int i = 0; i < lenBytes && currentReadIndex < tlvRaw.size(); i += 1, currentReadIndex += 1) {
				length = (length << 8) | tlvRaw[currentReadIndex];
			}
		}

		/* ѕрисваиваем возвращаем посчитанную длину */
		return length;
	}

	std::vector<BerTLV> decodeChildrenTags(const std::vector<unsigned char>& tlvRaw, size_t& currentReadIndex) {
		size_t currentReadIndexChildren = 0;
		std::vector<BerTLV> children;
		while (currentReadIndexChildren != berLen) {
			/* «апоминаем, какой глобальный индекс перед декодированием тега */
			size_t beforeDecode = currentReadIndex;

			/* ƒекодируем тег и добавл€ем его в массив каскадировани€ */
			BerTLV nextTag(tlvRaw, currentReadIndex);
			children.push_back(nextTag);

			/* ¬ычисл€ем количество прочитанных байтов */
			currentReadIndexChildren += currentReadIndex - beforeDecode;
		}

		return children;
	}
};