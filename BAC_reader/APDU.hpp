#pragma once
#include "BerTLV.hpp"
#include "Crypto.hpp"
#include "Util.hpp"

class APDU {
protected:
	union {
		UINT8 commandRaw[4];
		UINT32 commandTag;
	} commandHeader;
	vector<UINT8> commandData;
	UINT16 Le;
public:
	enum Fields { CLA, INS, P1, P2 };

	APDU(UINT8 CLA = 0, UINT8 INS = 0, UINT8 P1 = 0, UINT8 P2 = 0, const vector<UINT8>& data = {}, UINT16 le = 0) {
		commandHeader.commandRaw[APDU::Fields::CLA] = CLA;
		commandHeader.commandRaw[APDU::Fields::INS] = INS;
		commandHeader.commandRaw[APDU::Fields::P1] = P1;
		commandHeader.commandRaw[APDU::Fields::P2] = P2;
		commandData = data;
		Le = le;
	}
	void setCommandField(APDU::Fields field, UINT8 val) {
		commandHeader.commandRaw[field] = val;
	}
	void setCommandData(const vector<UINT8>& newData) {
		commandData = newData;
	}
	void appendCommandData(const vector<UINT8>& addData) {
		commandData.insert(commandData.end(), addData.begin(), addData.end());
	}
};
class SecureAPDU : public APDU {
	vector<UINT8> commandMac;

	BerTLV::BerTLVCoderToken generateDO87(const vector<UINT8>& ksEnc) {
		/* Получим алгоритм шифрования */
		Crypto::Des3& des3 = Crypto::getDes3Alg();

		/* Создадим тег для DO87 */
		BerTLV::BerTLVCoderToken tokenDO87(0x87);

		/* Если длина данных равна нулю, то DO87 не генерируется */
		if (this->commandData.size() != 0) {
			/* Сначала зашифруем данные */
			vector<UINT8> dataFilled = Crypto::fillISO9797(this->commandData, des3.getBlockLen());
			vector<UINT8> encrypted = des3.encrypt(dataFilled, ksEnc);

			/* Добавление 0x01 в начале зашифрованных данных; так в стандарте сказано */
			encrypted.insert(encrypted.begin(), 0x01);

			/* Добавим зашифрованные данные в тег 0x87 */
			tokenDO87.addData(encrypted);
		}

		return tokenDO87;
	}

	BerTLV::BerTLVCoderToken generateDO97(UINT16 Le) {
		/* Создаём тег 0x97, в котором будет лежать длина */
		BerTLV::BerTLVCoderToken tokenDO97(0x97);

		/* Если длина ожидаемых данных не ноль */
		if (Le != 0) {
			/* Получаем отображение числа в памяти */
			UINT8 NeRaw[sizeof(Le)] = { 0 };
			memcpy(NeRaw, &Le, sizeof(Le));

			/* Ищём первый ненулевой байт в этом числе */
			UINT8 nonZeroIndex = 0;
			for (; nonZeroIndex < sizeof(Le) && !NeRaw[nonZeroIndex]; nonZeroIndex += 1);

			/* Получаем длину числа в байтах */
			UINT32 neRawLen = sizeof(Le) - nonZeroIndex;

			/* Добавляем в токен байты длины */
			tokenDO97.addData(NeRaw + nonZeroIndex, neRawLen);
		}

		return tokenDO97;
	}

	BerTLV::BerTLVCoderToken generateDO8E(const vector<UINT8>& ksMac, UINT64& SSC, const BerTLV::BerTLVCoderToken& DO87, const BerTLV::BerTLVCoderToken& DO97) {
		Crypto::RetailMAC& mac = Crypto::getMacAlg();

		/* Увеличиваем SSC на 1 */
		SSC += 1;

		/* Получаем его байты и генерируем конкатенацию SSC, заполненной команды, DO87 и DO97 */
		vector<UINT8> concat = Util::castToVector(SSC);

		/* Добавляем байты команды для генерирования заполнения */
		for (INT i = 0; i < sizeof(this->commandHeader.commandRaw); i += 1) {
			concat.push_back(this->commandHeader.commandRaw[i]);
		}

		/* Генерация заполнения */
		concat = Crypto::fillISO9797(concat, mac.getBlockLen());

		/* После заполнения добавляем DO87 и DO97 (конечно, если они не нулевые) */
		if (DO87.getDataLen() != 0) {
			vector<UINT8> encDO87 = DO87.encode();
			concat.insert(concat.end(), encDO87.begin(), encDO87.end());
		}
		if (DO97.getDataLen() != 0) {
			vector<UINT8> encDO97 = DO97.encode();
			concat.insert(concat.end(), encDO97.begin(), encDO97.end());
		}

		/* Без заполнения, потому что алгоритм getMAC сам это умеет делать */
		commandMac = mac.getMAC(concat, ksMac);

		/* Формируем BER-TLV тег с номером 0x8E */
		BerTLV::BerTLVCoderToken tokenDO8E(0x8E);
		tokenDO8E.addData(commandMac);
		return tokenDO8E;
	}
public:
	/* Выполняется копирование данных исходной APDU команды */
	SecureAPDU(const APDU& apduCommand) : APDU(apduCommand) {
		/* Маскируем байт класса, по стандарту */
		this->setCommandField(APDU::Fields::CLA, 0x0C);
	}

	vector<UINT8> generateRawSecureAPDU(const vector<UINT8>& ksEnc, const vector<UINT8>& ksMac, UINT64& SSC) {
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
		BerTLV::BerTLVCoderToken apduToken(Util::changeEndiannes(this->commandHeader.commandTag));

		/* Если данные не нулевые - то добавить */
		if (tokenDO87.getDataLen() != 0) {
			vector<UINT8> encTokenDO87 = tokenDO87.encode();
			apduToken.addData(encTokenDO87);
		}
		if (tokenDO97.getDataLen() != 0) {
			vector<UINT8> encTokenDO97 = tokenDO97.encode();
			apduToken.addData(encTokenDO97);
		}
		if (tokenDO8E.getDataLen() != 0) {
			vector<UINT8> encTokenDO8E = tokenDO8E.encode();
			apduToken.addData(encTokenDO8E);
		}

		/* Формируем команду APDU */
		vector<UINT8> apduCommand = apduToken.encode();

		/* Получаем байты ожидаемой длины сообщения и вставляем их в конец закодированного APDU сообщения */
		vector<UINT8> leUINT8s = Util::castToVector(this->Le);
		apduCommand.insert(apduCommand.end(), leUINT8s.begin(), leUINT8s.end());
		return apduCommand;
	}

	vector<UINT8> getMessageMac() { return commandMac; }
};

class RAPDU {
protected:
	UINT8 SW1;
	UINT8 SW2;
	vector<UINT8> responceData;

public:
	RAPDU(const vector<UINT8>& responce) {
		UINT64 responceSize = responce.size();
		SW1 = responce[responceSize - 2];
		SW2 = responce[responceSize - 1];
		
		/* Копируем данные ответа */
		copy(responce.begin(), responce.begin() + responceSize - 2, responceData.begin());
	}

	UINT64 getResponceDataSize() const {
		return responceData.size();
	}
	vector<UINT8> getResponceData() const {
		return responceData;
	}

	bool isResponceOK() const {
		return (SW1 == 0x90) && (SW2 == 0x00);
	}
};
class SecureRAPDU : private RAPDU {
	bool isValid;

	vector<UINT8> responceDO87Data;
	vector<UINT8> responceDO99Data;
public:
	SecureRAPDU(const vector<UINT8>& responce, UINT64& SSC, const vector<BYTE>& ksMAC) : RAPDU(responce) {
		/* Получив ответ в виде защищённого RAPDU, его стоит дампнуть во временный файл */
		BerTLV::BerStream berStream(tmpfile());

		/* Записываем полученные данные в файл, а затем возвращаем каретку для считывания в начало файлаы */
		berStream.write(this->responceData.data(), this->responceData.size());
		berStream.seekg(0);
		
		/* Выполняем декодирование BER */
		BerTLV::BerTLVDecoder bertlvDecoder;
		UINT16 rootIndex = bertlvDecoder.decode(berStream);

		/* Пробуем найти теги 0x87, 0x97, 0x99 */
		UINT16 do87Index = bertlvDecoder.getChildByTag(rootIndex, 0x87);
		UINT16 do8EIndex = bertlvDecoder.getChildByTag(rootIndex, 0x8E);
		UINT16 do99Index = bertlvDecoder.getChildByTag(rootIndex, 0x99);

		/* Тег 0x8E всегда должен быть */
		if (do87Index == 0) {
			throw std::exception("Ошибка: SRAPDU получен без 0x8E");
		}

		/* Составляем конкатенацию данных на основе того, какие теги найдены */
		vector<UINT8> concat = Util::castToVector(SSC);
		
		if (do87Index) {
			vector<UINT8> rawDO87 = bertlvDecoder.getTokenRaw(berStream, do87Index);
			concat.insert(concat.end(), rawDO87.begin(), rawDO87.end());
		}
		if (do99Index) {
			vector<UINT8> rawDO99 = bertlvDecoder.getTokenRaw(berStream, do99Index);
			concat.insert(concat.end(), rawDO99.begin(), rawDO99.end());
		}

		/* Получим MAC сообщения, который нам передали */
		vector<UINT8> responceMAC = bertlvDecoder.getTokenData(berStream, do8EIndex);

		/* Посчитаем MAC сообщения сами */
		Crypto::RetailMAC& mac = Crypto::getMacAlg();
		vector<UINT8> computedMAC = mac.getMAC(concat, ksMAC);

		isValid = (computedMAC == responceMAC);
	}
	
	bool isResponceOK() const {
		return (SW1 == 0x90) && (SW2 == 0x00) && isValid;
	}
};