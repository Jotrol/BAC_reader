#pragma once

#include "Util.hpp"

#include <map>
#include <vector>
#include <fstream>
using namespace std;

namespace BerTLV {
	typedef basic_fstream<unsigned char> BerStream;

	/* Классы тега */
	enum class BerTagClass { UNIVERSAL, APPLICATION, CONTEXT_SPECIFIC, PRIVATE };

	/* Тип тега */
	enum class BerTagType { PRIMITIVE, CONSTRUCTED };

	/* Класс токена одного BER-TLV тега */
	class BerTLVDecoderToken {
	private:
		/* Пару typedef'ов, чтобы было удобней */
		typedef tuple<BerTagClass, BerTagType, UINT64> BerTagTuple;
		typedef tuple<UINT32, UINT32> BerLenTuple;

		/* Тег токена */
		UINT64 berTag;

		/* Длина тега */
		UINT32 berLen;

		/* Смещение данных тега внутри потока */
		UINT32 berDataStart;

		/* Класс тега */
		BerTagClass berTagClass;

		/* Тип тега */
		BerTagType berTagType;

		/* Индекс родителя тега */
		INT16 berParentIndex;

		/* Функция получения тега */
		BerTagTuple decodeTag(BerStream& stream) {
			/* Сначала получаем байт тега */
			UINT8 firstByte = stream.get();

			/* Определяем класс и тип тега */
			BerTagClass berTagClassRaw = (BerTagClass)((firstByte >> 6) & 0x03);
			BerTagType berTagTypeRaw = (BerTagType)((firstByte >> 5) & 0x01);

			/* Получаем сам номер тега */
			UINT64 berTagRaw = firstByte;

			/* Если 5 бит тега не равны единицам - то это единственный тег */
			if ((firstByte & 0x1F) == 0x1F) {
				/* Иначе, надо получить полный номер тега */
				for (UINT i = 0; i < 8; i += 1) {
					/* Получаем очередной байт тега */
					UINT8 tagNum = stream.get();

					/* Записываем его в итоговое число тега */
					berTagRaw = (berTagRaw << 8) | tagNum;

					/* Смотрим, установлен ли старший бит, если нет - это конец номера тега */
					if (((tagNum >> 7) & 0x01) == 0) { i = 8; }
				}
			}

			/* Агрегируем данные в неизменяемый многотиповой массив - tuple */
			return make_tuple(berTagClassRaw, berTagTypeRaw, berTagRaw);
		}

		/* Функция получения  */
		BerLenTuple decodeLength(BerStream& stream) {
			/* Считываем первый байт длины */
			UINT8 firstByte = stream.get();

			/* Если в первом байте длины установлен старший бит */
			/* То длина продолжается в других байтах */
			/* Кол - во равно первому байту(без старшего бита) */
			UINT32 berLenRaw = firstByte;
			if ((firstByte & 0x80) == 0x80) {
				/* Получаем кол-во байтов длины */
				UINT8 lenBytes = (firstByte & 0x7F);

				/* Пусть максимально можно будет задать 4 байта длиной */
				if (lenBytes >= sizeof(berLen)) {
					throw std::exception("Ошибка: длина данных больше, чем тип, способный её уместить");
				}

				/* Добавляем эти байты, пока не дошли до нуля */
				berLenRaw = 0;
				for (UINT8 i = 0; i < lenBytes; i += 1) {
					berLenRaw = (berLenRaw << 8) | (UINT8)stream.get();
				}
			}

			/* Агрегируем начало длину данных тега */
			return make_tuple((UINT32)stream.tellg(), berLenRaw);
		}

	public:
		BerTLVDecoderToken() = default;

		BerTLVDecoderToken(BerStream& stream, INT16 parentIdx) {
			/* Декодируем тег */
			BerTagTuple tagTuple = decodeTag(stream);

			/* Декодируем длину данных */
			BerLenTuple lenTuple = decodeLength(stream);

			/* Присваиваем всю информацию */
			berTagClass = get<0>(tagTuple);
			berTagType = get<1>(tagTuple);
			berTag = get<2>(tagTuple);

			berDataStart = get<0>(lenTuple);
			berLen = get<1>(lenTuple);

			berParentIndex = parentIdx;

			/* Если это не составной тег, то его данные можно пропустить */
			if (berTagType == BerTagType::PRIMITIVE) {
				stream.seekg(berLen, ios::cur);
			}
		}

		/* Проверка на то, что тег составной */
		bool isConstructed() { return berTagType == BerTagType::CONSTRUCTED; };

		/* Получить тег */
		UINT64 getTag() { return berTag; }

		/* Получить начало данных */
		UINT32 getDataStart() { return berDataStart; }

		/* Получить длину данных */
		UINT32 getDataLen() { return berLen; }

		/* Получить родителя тега */
		UINT16 getParent() { return berParentIndex; }

		/* В теории, можно сделать класс декодера дружественным этому */
		/* Чтобы избежать лишних функций */
	};

	/* Класс декодировщика BER-TLV данных */
	class BerTLVDecoder {
	private:
		/* Функция декодирования, которая вызывается типа рекурсивно */
		UINT16 decode(BerStream& stream, INT16 parentIndex, UINT32 parentTokenEnd) {
			/* Получив конец данных, функция знает, когда стоит остановиться */
			while (stream.tellg() < parentTokenEnd) {
				/* Создать новый токен */
				BerTLVDecoderToken token(stream, parentIndex);

				/* Добавить в массив токенов */
				tokens[tokenSize++] = token;

				/* Проверить, не закончилось ли место для токенов */
				checkForExtend();

				/* Если токен составной */
				if (token.isConstructed()) {
					/* Начать декодировать токены, определив */
					/* Новый конец и индекс родителя равный индеску текущего токена */
					decode(stream, tokenSize - 1, token.getDataLen() + (UINT32)stream.tellg());
				}
			}

			/* Вернуть ноль в головную функцию - это указывает на начало массива токенов */
			return 0;
		}

		/* Функция проверки оставшегося места в массиве токенов */
		void checkForExtend() {
			/* Если размер токенов не превышает максимального размера */
			if (tokenSize < tokenMaxSize) {
				/* То делать ничего не надо */
				return;
			}

			/* Иначе вдвое расширим максимальную вместимость */
			tokenMaxSize *= 2;

			/* Выделим кусок памяти с новой вместимостью */
			auto newPiece = make_unique<BerTLVDecoderToken[]>(tokenMaxSize);

			/* Скопируем старый кусок памяти в новый */
			memcpy(newPiece.get(), tokens.get(), tokenSize * sizeof(BerTLVDecoderToken));

			/* Освободим старую память */
			tokens.release();

			/* Обменяем указатели - теперь newPiece = nullptr */
			/* А tokens - хранит в себе новый массив токенов */
			tokens.swap(newPiece);
		}
	public:
		/* Максимальный размер массива токенов */
		UINT16 tokenMaxSize = 8;

		/* Текущий размер массива токенов */
		UINT16 tokenSize = 0;

		/* Сам массив токенов */
		unique_ptr<BerTLVDecoderToken[]> tokens;

		/* Конструктор декодера */
		BerTLVDecoder() : tokens(new BerTLVDecoderToken[tokenMaxSize]) {}

		/* Функция декодирования */
		UINT16 decode(BerStream& stream) {
			/* Получаем размер потока данных */
			stream.seekg(0, ios::end);
			UINT32 streamLen = (UINT32)stream.tellg();
			stream.seekg(0);

			/* Обнуляем текущий размер токенов */
			tokenSize = 0;
			return decode(stream, 0, streamLen);
		}

		/* Способ получить индекс токена по его тегу внутри составного тега */
		INT16 getChildByTag(INT16 parent, UINT64 childTag) {
			/* Начинаем с первого тега под родителем (так как в массиве они находятся подряд) */
			for (UINT16 i = parent; i < tokenSize; i += 1) {
				/* Получаем токен по индексу */
				BerTLVDecoderToken* token = &tokens[i];

				/* И проверяем его соответствие */
				if (token->getTag() == childTag && token->getParent() == parent) {
					/* Возвращаем индекс этого токена, если это тот, что нужен */
					return i;
				}
			}

			/* Иначе, если не найден токен, вернуть ноль */
			return -1;
		}

		/* Получение самого токена (его указателя, если вернее) */
		BerTLVDecoderToken* getTokenPtr(UINT16 tokenIndex) {
			/* Если каким-то образом индекс больше текущего размера массива */
			if (tokenIndex > tokenSize) {
				/* То вернуть нулевой указатель */
				return nullptr;
			}

			/* Во всех остальных случаях, вернуть адрес токена по индексу */
			return &tokens[tokenIndex];
		}

		vector<UINT8> getTokenRaw(BerStream& file, UINT16 tokenIndex) {
			/* Если каким-то образом индекс больше текущего размера массива */
			if (tokenIndex > tokenSize) {
				/* То вернуть нулевой указатель */
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

		/* Функция для получения данных тега */
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
			/* Определяем старший байт, который не равен нулю */
			/* Так как поменяли расположение байтов на bigendian */
			/* Значит, самый младший - последний, а старший - первый */
			/* Однако есть нулевые байты. Среди них и надо отыскать первый ненулевой */
			UINT8 berTagRaw[sizeof(UINT64)] = { 0 };
			newBerTag = Util::changeEndiannes(newBerTag);
			memcpy(berTagRaw, &newBerTag, sizeof(UINT64));

			UINT8 nonZero = getFirstNonZeroByteIndex(berTagRaw, sizeof(UINT64));

			/* Найдя такой байт, берём его и получаем класс тега */
			UINT8 firstByte = berTagRaw[nonZero];
			berTagType = (BerTagType)((firstByte >> 5) & 0x01);

			/* Копируем байты тега в массив в bigendian */
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
			/* Кодируем данные тега */
			vector<UINT8> valEnc = encodeValue();

			/* Если данных нет - то нет и тега, логично? */
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