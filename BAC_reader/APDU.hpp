#pragma once
#include "BerTLV.hpp"
#include "Crypto.hpp"
#include "Util.hpp"

#include <sstream>
#include <string>

namespace APDU {
	class APDU {
	protected:
		UINT8 commandRaw[4];
		ByteVec commandData;
		UINT16 Le;

		/* Перевод байтов длины в вектор */
		ByteVec getLeBytes(UINT16 Le) const {
			/* Получаем каждый байт отдельно */
			UINT8 byte1 = (Le >> 8) & 0xFF;
			UINT8 byte2 = Le & 0xFF;

			/* Возвращем либо один, либо два байта */
			/* Зависит от того, каков первый байт */
			if (byte1) { return { byte1, byte2 }; }
			return { byte2 };
		}
	public:
		/* Поля APDU запроса */
		enum Fields { CLA, INS, P1, P2 };

		APDU(UINT8 CLA = 0, UINT8 INS = 0, UINT8 P1 = 0, UINT8 P2 = 0, const ByteVec& data = {}, UINT16 le = 0) {
			commandRaw[APDU::Fields::CLA] = CLA;
			commandRaw[APDU::Fields::INS] = INS;
			commandRaw[APDU::Fields::P1] = P1;
			commandRaw[APDU::Fields::P2] = P2;
			commandData = data;
			Le = le;
		}

		/* Установка поля */
		void setCommandField(APDU::Fields field, UINT8 val) {
			commandRaw[field] = val;
		}

		/* Установка длины */
		void setLe(UINT16 le) {
			Le = le;
		}

		/* Ставим (заменяя) данные запроса */
		void setCommandData(const ByteVec& newData) {
			commandData = newData;
		}

		/* Дописываем к данным запроса */
		void appendCommandData(const ByteVec& addData) {
			commandData.insert(commandData.end(), addData.begin(), addData.end());
		}

		/* Получение команды в виде вектора для отправки */
		ByteVec getCommand() const {
			ByteVec leBytes = getLeBytes(Le);
			ByteVec command = { commandRaw[0], commandRaw[1], commandRaw[2], commandRaw[3] };
			command.insert(command.end(), commandData.begin(), commandData.end());
			command.insert(command.end(), leBytes.begin(), leBytes.end());
			return command;
		}
	};

	/* Безопасный запрос, конструируется из небезопасного */
	class SecureAPDU : public APDU {
	private:
		/* Генерация DO'87' */
		BerTLV::BerTLVCoderToken generateDO87(const ByteVec& ksEnc) {
			/* Получим алгоритм шифрования */
			Crypto::Des3& des3 = Crypto::getDes3Alg();

			/* Создадим тег для DO87 */
			BerTLV::BerTLVCoderToken tokenDO87(0x87);

			/* Если длина данных равна нулю, то DO87 не генерируется */
			if (this->commandData.size() != 0) {
				/* Сначала зашифруем данные */
				ByteVec dataFilled = Crypto::fillISO9797(this->commandData, des3.getBlockLen());
				ByteVec encrypted = des3.encrypt(dataFilled, ksEnc);

				/* Добавление 0x01 в начале зашифрованных данных; так в стандарте сказано */
				encrypted.insert(encrypted.begin(), 0x01);

				/* Добавим зашифрованные данные в тег 0x87 */
				tokenDO87.addData(encrypted);
			}

			return tokenDO87;
		}

		/* Генерация DO'97' */
		BerTLV::BerTLVCoderToken generateDO97(UINT16 Le) {
			/* Создаём тег 0x97, в котором будет лежать длина */
			BerTLV::BerTLVCoderToken tokenDO97(0x97);

			/* Если длина ожидаемых данных не ноль */
			if (Le != 0) {
				/* Добавляем в токен байты длины */
				tokenDO97.addData(getLeBytes(Le));
			}

			return tokenDO97;
		}

		/* Генерация DO'8E' */
		BerTLV::BerTLVCoderToken generateDO8E(const ByteVec& ksMac, UINT64& SSC, const BerTLV::BerTLVCoderToken& DO87, const BerTLV::BerTLVCoderToken& DO97) {
			Crypto::RetailMAC& mac = Crypto::getMacAlg();

			/* Увеличиваем SSC на 1 */
			SSC += 1;

			/* Получаем его байты и генерируем конкатенацию SSC, заполненной команды, DO87 и DO97 */
			ByteVec concat = Util::castToVector(SSC);

			/* Добавляем байты команды для генерирования заполнения */
			for (INT i = 0; i < sizeof(this->commandRaw); i += 1) {
				concat.push_back(this->commandRaw[i]);
			}

			/* Генерация заполнения */
			concat = Crypto::fillISO9797(concat, mac.getBlockLen());

			/* После заполнения добавляем DO87 и DO97 (конечно, если они не нулевые) */
			if (DO87.getDataLen() != 0) {
				ByteVec encDO87 = DO87.encode();
				concat.insert(concat.end(), encDO87.begin(), encDO87.end());
			}
			if (DO97.getDataLen() != 0) {
				ByteVec encDO97 = DO97.encode();
				concat.insert(concat.end(), encDO97.begin(), encDO97.end());
			}

			/* Без заполнения, потому что алгоритм getMAC сам это умеет делать */
			ByteVec commandMac = mac.getMAC(concat, ksMac);

			/* Формируем BER-TLV тег с номером 0x8E */
			BerTLV::BerTLVCoderToken tokenDO8E(0x8E);
			tokenDO8E.addData(commandMac);
			return tokenDO8E;
		}
	public:
		SecureAPDU() = default;

		/* Выполняется копирование данных исходной APDU команды */
		SecureAPDU(const APDU& apduCommand) : APDU(apduCommand) {
			/* Маскируем байт класса, по стандарту */
			this->setCommandField(APDU::Fields::CLA, 0x0C);
		}

		vector<UINT8> generateRawSecureAPDU(const ByteVec& ksEnc, const ByteVec& ksMac, UINT64& SSC) {
			/* Генерируем части APDU ответа в виде токенов BER-TLV */
			BerTLV::BerTLVCoderToken tokenDO87 = this->generateDO87(ksEnc);
			BerTLV::BerTLVCoderToken tokenDO97 = this->generateDO97(this->Le);
			BerTLV::BerTLVCoderToken tokenDO8E = this->generateDO8E(ksMac, SSC, tokenDO87, tokenDO97);

			/* Добавляем закодированные данные каждого токена в данные APDU тега */
			/* У токенов прикол: они переводят число в Big Endian сами */
			/* При этом, changeEndiannes реализовано так, что на BE машинах не происходит обмена байтами */
			/* По этому, чтобы гарантировать однозначность, надо добавить эту функцию сюда */
			/* Ибо после заполнения commandRaw в конструкторе APDU, тег станет на любой машине BE */
			/* А на вход функции в LE машинах предполагается LE число. На BE предполагается BE */
			/* но функция changeEndiannes на BE машинах ничего не делает */
			UINT32 commandTag = (commandRaw[0] << 24) | (commandRaw[1] << 16) | (commandRaw[2] << 8) | commandRaw[3];
			BerTLV::BerTLVCoderToken apduToken(commandTag);

			/* Если данные не нулевые - то добавить */
			if (tokenDO87.getDataLen() != 0) {
				ByteVec encTokenDO87 = tokenDO87.encode();
				apduToken.addData(encTokenDO87);
			}
			if (tokenDO97.getDataLen() != 0) {
				ByteVec encTokenDO97 = tokenDO97.encode();
				apduToken.addData(encTokenDO97);
			}
			if (tokenDO8E.getDataLen() != 0) {
				ByteVec encTokenDO8E = tokenDO8E.encode();
				apduToken.addData(encTokenDO8E);
			}

			/* Формируем команду APDU */
			ByteVec apduCommand = apduToken.encode();

			/* Получаем байты ожидаемой длины сообщения и вставляем их в конец закодированного APDU сообщения */
			ByteVec leUINT8s = getLeBytes(this->Le);
			apduCommand.insert(apduCommand.end(), leUINT8s.begin(), leUINT8s.end());
			return apduCommand;
		}
	};

	/* Незащищённый ответ */
	class RAPDU {
	protected:
		UINT8 SW1 = 0;
		UINT8 SW2 = 0;
		ByteVec responceData;

	public:
		RAPDU() = default;
		RAPDU(const ByteVec& responce) {
			UINT64 responceSize = responce.size();
			SW1 = responce[responceSize - 2];
			SW2 = responce[responceSize - 1];

			responceData = {};

			/* Если есть данные - т.е длина ответа больше 2 */
			if (responce.size() > 2) {
				responceData.insert(responceData.begin(), responce.begin(), responce.begin() + responceSize - 2);
			}
		}
		
		/* Узнаём размер данных ответа */
		UINT64 getResponceDataSize() const {
			return responceData.size();
		}

		/* Удобная функция оповещения о том, что вернул запрос */
		std::string report() {
			std::stringstream ss;
			ss << "SW1=" << hex << (int)SW1 << ",SW2=" << hex << (int)SW2;
			return ss.str();
		}

		/* Получение данных ответа */
		ByteVec getResponceData() const {
			return responceData;
		}

		/* Успешен ли запрос */
		virtual bool isResponceOK() const {
			return (SW1 == 0x90) && (SW2 == 0x00);
		}
	};

	/* Защищённый ответ */
	class SecureRAPDU : public RAPDU {
		bool isValid = false;
		ByteVec responceDO87DecodedData;
	public:
		SecureRAPDU() = default;
		SecureRAPDU(const ByteVec& responce, const ByteVec& ksEnc, const ByteVec& ksMAC, UINT64& SSC) : RAPDU(responce) {
			/* Получив ответ в виде защищённого RAPDU, его стоит дампнуть во временный файл */
		#ifdef _DEBUG
			BerTLV::BerStream berStream("apduRaw.bin", ios::in | ios::out | ios::binary | ios::trunc);
		#else
			/* Откроем временный файл, чтобы он сам удалился после закрытия */
			BerTLV::BerStream berStream(tmpfile());
		#endif

			/* Записываем полученные данные в файл, а затем возвращаем каретку для считывания в начало файлаы */
			berStream.write(this->responceData.data(), this->responceData.size());
			berStream.seekg(0);

			/* Выполняем декодирование BER */
			BerTLV::BerTLVDecoder bertlvDecoder;
			UINT16 rootIndex = bertlvDecoder.decode(berStream);

			/* Пробуем найти теги 0x87, 0x97, 0x99 */
			INT16 do87Index = bertlvDecoder.getChildByTag(rootIndex, 0x87);
			INT16 do8EIndex = bertlvDecoder.getChildByTag(rootIndex, 0x8E);
			INT16 do99Index = bertlvDecoder.getChildByTag(rootIndex, 0x99);

			/* Тег 0x8E всегда должен быть */
			if (do8EIndex == -1) {
				throw std::exception("SRAPDU получен без 0x8E");
			}

			/* Составляем конкатенацию данных на основе того, какие теги найдены */
			SSC += 1;
			ByteVec concat = Util::castToVector(SSC);

			if (do87Index != -1) {
				ByteVec rawDO87 = bertlvDecoder.getTokenRaw(berStream, do87Index);
				concat.insert(concat.end(), rawDO87.begin(), rawDO87.end());
			}
			if (do99Index != -1) {
				ByteVec rawDO99 = bertlvDecoder.getTokenRaw(berStream, do99Index);
				concat.insert(concat.end(), rawDO99.begin(), rawDO99.end());
			}

			/* Получим MAC сообщения, который нам передали */
			ByteVec responceMAC = bertlvDecoder.getTokenData(berStream, do8EIndex);

			/* Посчитаем MAC сообщения сами */
			Crypto::RetailMAC& mac = Crypto::getMacAlg();
			ByteVec computedMAC = mac.getMAC(concat, ksMAC);

			isValid = (computedMAC == responceMAC);
			if (!isValid) {
				return;
			}

			/* Если присутствует тег данных */
			if (do87Index != -1) {
				responceData = bertlvDecoder.getTokenData(berStream, do87Index);

				/* Получаем алгоритм шифрования */
				Crypto::Des3& des3 = Crypto::getDes3Alg();

				/* Избавляемся от 0x01 в DO'87 */
				responceDO87DecodedData.insert(responceDO87DecodedData.end(), responceData.begin() + 1, responceData.end());

				responceDO87DecodedData = des3.decrypt(responceDO87DecodedData, ksEnc);

				/* Убираем заполнение */
				INT find0x80 = -1;
				for (INT i = responceDO87DecodedData.size() - 1; i > -1; i -= 1) {
					if (responceDO87DecodedData[i] == 0x80) {
						find0x80 = i;
						break;
					}
				}

				/* Если не получилось найти 0x80 - то заполнения не было и это невалидные данные */
				if (find0x80 == -1) {
					responceDO87DecodedData = {};
				}
				else {
					responceDO87DecodedData.resize(find0x80);
				}
			}

			berStream.close();
		}

		/* Успешен ли запрос и при этом, верен ли */
		virtual bool isResponceOK() const {
			return (SW1 == 0x90) && (SW2 == 0x00) && isValid;
		}

		/* Получаем расшифрованные данные ответа */
		ByteVec getDecodedResponceData() const {
			return responceDO87DecodedData;
		}
	};
}