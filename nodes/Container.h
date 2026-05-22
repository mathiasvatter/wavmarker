#pragma once

#include "nodes/Reflection.h"

class FileInputStream;
class FileOutputStream;

struct Container : Reflectable {
	virtual ~Container() = default;
	virtual void parse(FileInputStream& in) = 0;
	virtual void write(FileOutputStream& out) = 0;
};
