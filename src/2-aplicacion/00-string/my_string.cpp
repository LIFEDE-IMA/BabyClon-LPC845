/*
 * my_string.cpp
 *
 *  Created on: 6 jun. 2026
 *      Author: Mati3 - LIFEDE - UTN FRBA
 *      Consultas: mmelian@frba.utn.edu.ar
 */

#include "my_string.h"

String::String(char *str, uint16_t size){
	m_str = str;
	MAX_LEN = size;
	m_index = 0;
	m_error = error_type::OK;
	m_str[0] = '\0';
}

uint16_t String::strlen(const char *str){
    uint16_t i = 0;
    while(*str != '\0'){
        i++;
        str++;
    }
    return i;
}

uint16_t String::strlen(char *str){
    uint16_t i = 0;
    while(*str != '\0'){
        i++;
        str++;
    }
    return i;
}

void String::strcpy(char *dest, const char *src){
    while(*src != '\0'){
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

void String::strcpy(char *dest, char *src){
    while(*src != '\0'){
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

int String::strcmp(const char *s1, const char *s2){
	while((*s1 != '\0') && (*s2 != '\0')){
		if(*s1 != *s2){
			return (*s1 - *s2);	//	< 0 if s1 < s2, > 0 if s1 > s2
		}
		s1++;
		s2++;
	}
	return (*s1 - *s2);	//  0 if s1 == s2
}

const char* String::strstr(const char *s1, const char *s2){
    if(!*s2) return s1;

    while(*s1){
        const char *p1 = s1;
        const char *p2 = s2;

        while(*p1 && *p2 && (*p1 == *p2)){
            p1++;
            p2++;
        }
        if(!*p2){
            return s1;
        }
        s1++;
    }
    return nullptr;
}

const char* String::strstr(const char *s1, const char *s2, uint32_t s1Len){
    if(!*s2) return s1;

    uint32_t len = 0;

    while(len < s1Len){
        const char *p1 = s1;
        const char *p2 = s2;

        while(*p1 && *p2 && (*p1 == *p2)){
            p1++;
            p2++;
        }
        if(!*p2){
            return s1;
        }
        s1++;
        len++;
    }
    return nullptr;
}

const char* String::strchr(const char *s, char c){
	if(!s)	return nullptr;

	while(*s != '\0'){
		if(*s == c)	return s;
		s++;
	}
	return ((c == '\0') ? s : nullptr);
}

void String::uint_to_str(uint32_t val, char *str){
	char aux[11];			//	Max number: 2^32 - 1
	uint8_t i = 0, j = 0;

	if(val == 0){
		str[0] = '0';
		str[1] = '\0';
		return;
	}

	while(val){
		aux[i++] = ((val % 10) + '0');
		val /= 10;
	}

	while(i--){
		str[j++] = aux[i];
	}
	str[j] = '\0';
}

int String::float2str(float data, char *buffer, int initPos){
	int len = initPos;
	int j = 0;
	char str[13];

	if(data < 0){
		buffer[len++] = '-';
		data = -data;
	}

	int entero = (int)(data + 0.05f);
	int decimal = (int)((data - entero)*10);	//	1 decimal

	if (entero == 0){
		str[j++] = '0';
	}else{
		while(entero > 0){	//	Write integers to the string
			str[j++] = (entero % 10) + '0';
			entero /= 10;
		}
	}

	while(j--){	//	Reverse the string
		buffer[len++] = str[j];
	}

	buffer[len++] = '.';

	buffer[len++] = (decimal % 10) + '0';	//	Write decimal to the string

	buffer[len] = '\0';

	return len;
}

int String::floatVec2str(float *data, uint8_t dataSize, char *buffer, char delimiter){
	int len = 0;
	for(uint8_t i = 0; i < dataSize; i++){
		len = String::float2str(data[i], buffer, len);
		buffer[len] = delimiter;
		len++;
	}
	len--;
	buffer[len] = '\0';	//	buffer = data[0]'delimiter'data[1]...'delimiter'data[dataSize-1]

	return len;
}

const char* String::str_to_uint(const char *str, uint32_t &value, uint8_t maxDigits){
	if(!str) return nullptr;
	if(maxDigits > 10) maxDigits = 10;	//	Max Digits of an uint32_t

	uint32_t res = 0;
	uint8_t digits = 0;

	while((*str >= '0') && (*str <= '9')){
		if(digits >= maxDigits) return nullptr;	//	Too Many Digits
		res = ((res * 10) + (*str - '0'));
		digits++;
		str++;
	}

	if(digits == 0) return nullptr;	//	No Number in Str

	value = res;
	return str;
}

void String::append(const char *str){
	if(!str){
		m_error = error_type::NULL_STR;
		return;
	}

	uint16_t len = String::strlen(str);

	if(m_index + len >= MAX_LEN){
		m_error = error_type::BUFFER_OVERFLOW;
		return;
	}
	String::strcpy(m_str + m_index, str);	//	Adds str to existing m_str
	m_index += len;
	m_error = error_type::OK;
}

void String::append(char *str){
	if(!str){
		m_error = error_type::NULL_STR;
		return;
	}

	uint16_t len = String::strlen(str);

	if(m_index + len >= MAX_LEN){
		m_error = error_type::BUFFER_OVERFLOW;
		return;
	}
	String::strcpy(m_str + m_index, str);	//	Adds str to existing m_str
	m_index += len;
	m_error = error_type::OK;
}

void String::append(const char *str, uint16_t len){
	if(!str){
		m_error = error_type::NULL_STR;
		return;
	}

	if(m_index + len >= MAX_LEN){
		m_error = error_type::BUFFER_OVERFLOW;
		return;
	}

	for(uint16_t idx = 0; idx < len; idx++)
		m_str[(m_index + idx)] = str[idx];

	m_index += len;
	m_str[m_index] = '\0';
	m_error = error_type::OK;
}

const char* String::skipPrefix(const char *s, const char *prefix){
	if(!s || !prefix) return nullptr;

	while(*prefix != '\0'){
		if(*s != *prefix) return nullptr;
		s++;
		prefix++;
	}

	return s;
}

const char* String::findAfter(const char *s, const char *subStr){
	if(!s || !subStr) return nullptr;

	const char *p = String::strstr(s, subStr);
	if(!p) return nullptr;

	return p + String::strlen(subStr);
}

bool String::findBetween(const char *s, const char *startSubStr, const char *endSubStr, const char *&begin, uint16_t &len){
	if(!endSubStr) return false;

	const char *start = String::findAfter(s, startSubStr);
	if(!start) return false;

	const char *end = String::strstr(start, endSubStr);
	if(!end) return false;

	begin = start;
	len = (uint16_t)(end - start);
	return true;
}

int String::matchPrefix(const char *s, const char *const *options, uint8_t count, const char **end){
	if(!s || !options) return -1;

	for(uint8_t i = 0; i < count; i++){
		const char *after = String::skipPrefix(s, options[i]);
		if(after){
			if(end) *end = after;
			return i;
		}
	}
	return -1;
}

bool String::assign(const char *str, uint16_t len){
	if(!str){
		m_error = error_type::NULL_STR;
		return false;
	}

	if(len >= MAX_LEN){
		m_error = error_type::BUFFER_OVERFLOW;
		return false;
	}

	for(uint16_t i = 0; i < len; i++){
		m_str[i] = str[i];
	}
	m_str[len] = '\0';
	m_index = len;
	m_error = error_type::OK;
	return true;
}

String& String::operator=(const char *str){
	if(!str){
		m_error = error_type::NULL_STR;
		return *this;
	}

	String::strcpy(m_str, str);	//	Copies str to existing m_str
	m_index = String::strlen(str);
	m_error = error_type::OK;

	return *this;
}

String& String::operator=(char *str){
	if(!str){
		m_error = error_type::NULL_STR;
		return *this;
	}

	String::strcpy(m_str, str);	//	Copies str to existing m_str
	m_index = String::strlen(str);
	m_error = error_type::OK;

	return *this;
}

String& String::operator=(uint8_t val){
	char buffer[4];

	String::uint_to_str(val, buffer);
	String::strcpy(m_str, buffer);	//	Copies str to existing m_str
	m_index = String::strlen(buffer);
	m_error = error_type::OK;

	return *this;
}

String& String::operator=(uint16_t val){
	char buffer[6];

	String::uint_to_str(val, buffer);
	String::strcpy(m_str, buffer);	//	Copies str to existing m_str
	m_index = String::strlen(buffer);
	m_error = error_type::OK;

	return *this;
}

String& String::operator=(uint32_t val){
	char buffer[11];

	String::uint_to_str(val, buffer);
	String::strcpy(m_str, buffer);	//	Copies str to existing m_str
	m_index = String::strlen(buffer);
	m_error = error_type::OK;

	return *this;
}

String& String::operator +=(const char *str){
	String::append(str);
	return *this;
}

String& String::operator +=(char *str){
	String::append(str);
	return *this;
}

String& String::operator +=(uint8_t val){
	char buffer[4];

	String::uint_to_str(val, buffer);
	String::append(buffer);
	return *this;
}

String& String::operator +=(uint16_t val){
	char buffer[6];

	String::uint_to_str(val, buffer);
	String::append(buffer);
	return *this;
}

String& String::operator +=(uint32_t val){
	char buffer[11];

	String::uint_to_str(val, buffer);
	String::append(buffer);
	return *this;
}

String& String::operator+=(float val){
	char buffer[13];	//	MAX: uint32_t + 1 decimal +  "-"

	String::float2str(val, buffer);
	String::append(buffer);
	return *this;
}

bool String::operator ==(const char *str) const{
	return (!String::strcmp(m_str, str));
}

bool String::operator !=(const char *str) const{
	return String::strcmp(m_str, str);
}

void String::clear(){
	m_index = 0;
	m_str[0] = '\0';
	m_error = error_type::OK;
}

const char* String::getStr() const{
	return m_str;
}

uint16_t String::getLen() const{ return m_index; }

uint16_t String::getCapacity() const{ return (MAX_LEN - 1); }

uint8_t String::getError() const{ return m_error; }

String::~String(){}
