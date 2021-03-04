#ifndef MULTIPARAM_H
#define MULTIPARAM_H

/*
 * MultiParam: class to store sequence type specific parameter values
 * written by Annika Seidel <annika.seidel@mpibpc.mpg.de>
 */

#include <string>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <cfloat>
#include "Util.h"

template<class T>
class NuclAA {
public:
    static const std::string constFirst;
    static const std::string constSecond;
    static const std::string parseStr;
    T first;
    T second;

    NuclAA<T>(const NuclAA<T> &value) {
        this->first = value.first;
        this->second = value.second;
    }

    static const T max;

    NuclAA<T>() {}

    NuclAA<T>(T first) {
        this->first = first;
        this->second = first;
    }

    NuclAA<T>(T first, T second) {
        this->first = first;
        this->second = second;
    }

    NuclAA<T> &operator=(NuclAA<T> value) {
        this->first = value.first;
        this->second = value.second;
        return *this;
    }

    NuclAA<T> &operator=(T value) {
        this->first = value;
        this->second = value;
        return *this;
    }

    T nucleotide() const {
        return second;
    }

    void nucleotide(T val) {
        second = val;
    }

    T aminoacid() const {
        return first;
    }

    void aminoacid(T val) {
        first = val;
    }

    bool operator==(const T &other) const {
        return nucleotide() == other || aminoacid() == other;
    }

    bool operator!=(const T &other) const {
        return !(operator==(other));
    }
};


class PseudoCounts {
public:
    static const std::string constFirst;
    static const std::string constSecond;
    static const std::string parseStr;
    float first;
    float second;

    PseudoCounts(const PseudoCounts &value) {
        this->first = value.first;
        this->second = value.second;
    }
    static const float max;

    PseudoCounts() {}

    PseudoCounts(float first) {
        this->first = first;
        this->second = first;
    }

    PseudoCounts(float first, float second) {
        this->first = first;
        this->second = second;
    }

    PseudoCounts &operator=(PseudoCounts value) {
        this->first = value.first;
        this->second = value.second;
        return *this;
    }

    PseudoCounts &operator=(float value) {
        this->first = value;
        this->second = value;
        return *this;
    }

    float normal() const { return first; }
    void normal(float val) { first = val; }

    float cs() const { return second; }
    void cs(float val) { second = val; }
};


template<class T>
class MultiParam {
public:
    T values;

    MultiParam() {};

    MultiParam(T type) {
        values.first = type.first;
        values.second = type.second;
    }

    bool assign(const std::string &value, std::string &obj) {
        obj = value;
        return true;
    }

    bool assign(const std::string &value, int &obj) {
        char *rest = NULL;
        int tmp = strtol(value.c_str(), &rest, 10);
        if ((rest != value.c_str() && *rest != '\0') || errno == ERANGE) {
            return false;
        }
        obj = tmp;
        return true;
    }

    bool assign(const std::string &value, float &obj) {
        char *rest = NULL;
        float tmp = strtof(value.c_str(), &rest);
        if ((rest != value.c_str() && *rest != '\0') || errno == ERANGE) {
            return false;
        }
        obj = tmp;
        return true;
    }

    MultiParam(const char *parametercstring);

    static std::string format(const MultiParam<T> &file) {
        return T::constFirst + ":" + SSTR(file.values.first) + "," + T::constSecond + ":" + SSTR(file.values.second);
    }

    MultiParam<T> &operator=(T value) {
        this->values = value;
        return *this;
    }
};

#endif
