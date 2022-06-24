#pragma once

#include <string>
#include <random>

namespace Util {
#define IS_LITTLE_ENDIAN  ('ABCD'==0x41424344UL)

	/* ������ ��� ��������� ������ */
	/* � �����, ��������� � ��� ����� ��������, �� � �� ������� �� ������ ������ �� ���� */
	template<typename T>
	T changeEndiannes(T val) {
		return val;
	}

	/* ������������� ������ ��� Little Endian ����� */
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

	/* ������� ��� ��������� ���������� ID: �������� �� UUIDv4 */
	std::string getUUID() {
		/* �������� ������������ ���������� ��������� ����� */
		/* �� ������ ��������� �������� */
		static std::random_device dev;
		static std::mt19937 rng(dev());

		/* �������� ������������������������ ������� */
		std::uniform_int_distribution<int> dist(0, 15);

		/* ��������� ��������������� ���������� ID */
		const char v[] = "0123456789abcdef";
		char output[32] = { 0 };
		for (int i = 0; i < sizeof(output); i += 2) {
			output[i] = v[dist(rng)];
			output[i + 1] = v[dist(rng)];
		}

		return output;
	}

	template<typename T>
	vector<UINT8> castToVector(T val) {
		UINT8 valRaw[sizeof(T)] = { 0 };
		memcpy(valRaw, &val, sizeof(T));

		vector<UINT8> out;
		for (int i = sizeof(T) - 1; i > -1; i -= 1) {
			out.push_back(valRaw[i]);
		}
		return out;
	}
}