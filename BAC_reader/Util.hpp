#pragma once

#include <string>
#include <random>
#include <windef.h>

namespace Util {
#define IS_LITTLE_ENDIAN  ('ABCD'==0x41424344UL)

	/* Шаблон для разворота байтов */
	/* В целом, программа и без этого работает, но я бы убирать на всякий случай не стал */
	template<typename T>
	T changeEndiannes(T val) {
		return val;
	}

	/* Специализации только для Little Endian машин */
#if IS_LITTLE_ENDIAN
	template<typename T = UINT16>
	UINT16 changeEndiannes(UINT16 val) {
		return _byteswap_ushort(val);
	}

	template<typename T = UINT32>
	UINT32 changeEndiannes(UINT32 val) {
		return _byteswap_ulong(val);
	}

	template<typename T = UINT64>
	UINT64 changeEndiannes(UINT64 val) {
		return _byteswap_uint64(val);
	}
#endif

	/* Функция для получения случайного ID: основана на UUIDv4 */
	std::string getUUID() {
		/* Создание статического генератора случайных чисел */
		/* На основе смещателя Мерсенна */
		static std::random_device dev;
		static std::mt19937 rng(dev());

		/* Создание равномернораспределённого отрезка */
		std::uniform_int_distribution<int> dist(0, 15);

		/* Генерация непосредственно случайного ID */
		const char v[] = "0123456789abcdef";
		char output[32] = { 0 };
		for (int i = 0; i < sizeof(output); i += 2) {
			output[i] = v[dist(rng)];
			output[i + 1] = v[dist(rng)];
		}

		return output;
	}
}