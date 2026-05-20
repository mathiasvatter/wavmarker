#pragma once

class FileInputStream;
class FileOutputStream;

struct Container {
	virtual ~Container() = default;
	virtual void parse(FileInputStream& in) = 0;
	virtual void write(FileOutputStream& out) = 0;
};
