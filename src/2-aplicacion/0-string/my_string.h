/*
 * my_string.h
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#ifndef STRING_H_
#define STRING_H_

#include <stdint.h>

//	Modified "String" class so it can be used in micros code (doesnt use "new" or "delete")
class String{
	private:
		char *m_str;
		uint16_t MAX_LEN;
		uint16_t m_index;
		uint8_t m_error;

		void strcpy(char *dest, const char *src);	//	Copies src into dest
		void append(const char *str);				//	Concatenation

		void uint_to_str(uint32_t val, char *str);	//	Converts uint_t to string

	public:
		enum error_type{
			OK,
			BUFFER_OVERFLOW,
			NULL_STR
		};

		String(char *str, uint16_t size);							//	Constructor

		static uint16_t strlen(const char *str);					//	Gets str length
		static int strcmp(const char *s1, const char *s2);			//	Compares s1 with s2
		static const char* strstr(const char *s1, const char *s2);	//  Searches s2 in s1
		static int float2str(float data, char *buffer, int initPos = 0);		//	Converts float to string
		static int floatVec2str(float *data, uint8_t dataSize, char *buffer);	//	Converts float vector to string

		String& operator+=(const char *str);						//	Concatenation
		String& operator+=(uint8_t val);							//	Concatenation str + uint8_t
		String& operator+=(uint16_t val);							//	Concatenation str + uint16_t
		String& operator+=(uint32_t val);							//	Concatenation str + uint32_t
		bool operator==(const char *str) const;						//	Compares str with m_str
		bool operator!=(const char *str) const;						//	Compares str with m_str

		const char* getStr() const;									//	Gets the string
		uint16_t getLen() const;									//	Gets m_str length
		uint8_t getError() const;									//	Asks if an operation was succesfull
		void clear();												//	Deletes the string

		~String();													//	Destroyer
};

#endif /* STRING_H_ */
