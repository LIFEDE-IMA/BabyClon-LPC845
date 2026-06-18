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

void String::strcpy(char *dest, const char *src){
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

int String::float2str(float data, char *buffer, int initPos){
	int len = initPos;
	int j = 0;
	char str[11];

	if(data < 0){
		buffer[len++] = '-';
		data = -data;
	}

	int entero = (int)data;
	int decimal = (int)((data - entero)*1000);	//	3 decimals

	entero = data;

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

	buffer[len++] = ((decimal / 100) % 10) + '0';	//	Write decimals to the string
	buffer[len++] = ((decimal / 10) % 10) + '0';
	buffer[len++] = (decimal % 10) + '0';

	buffer[len] = '\0';

	return len;
}

int String::floatVec2str(float *data, uint8_t dataSize, char *buffer){
	int len = 0;
	for(uint8_t i = 0; i < dataSize; i++){
		len = String::float2str(data[i], buffer, len);
		buffer[len] = '&';
		len++;
	}
	len--;
	buffer[len] = '\0';	//	buffer = data[0]&data[1]...&data[dataSize-1]

	return len;
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

String& String::operator +=(const char *str){
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

uint8_t String::getError() const{ return m_error; }

String::~String(){}
