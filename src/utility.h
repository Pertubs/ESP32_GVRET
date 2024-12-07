#pragma once

class Utility
{
public:
    static unsigned int parseHexCharacter(char chr)
    {
        unsigned int result = 0;
        if (chr >= '0' && chr <= '9') result = chr - '0';
        else if (chr >= 'A' && chr <= 'F') result = 10 + chr - 'A';
        else if (chr >= 'a' && chr <= 'f') result = 10 + chr - 'a';

        return result;
    }

    static unsigned int parseHexString(char *str, int length)
    {
        unsigned int result = 0;
        for (int i = 0; i < length; i++) result += parseHexCharacter(str[i]) << (4 * (length - i - 1));
        return result;
    }

    float Safe_Division(float numerator, float denominator, float default_value = 0.0f) 
    {
    if (denominator == 0.0f) {
        // Divisor é zero, retorna o valor padrão para evitar crash
        return default_value;
    }
    return numerator / denominator;
}


};