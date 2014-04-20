#pragma once

#include <string>

using namespace std;

namespace flint {
	
	class StringPiece {
	private:
		const char *ptr_;
		size_t len_;

	public:
		StringPiece() : len_(0) {};
		StringPiece(const char *ptr, size_t len) : ptr_(ptr), len_(len) {};

		const char* data() const {
			return ptr_;
		};

		inline StringPiece subpiece(size_t start, size_t len) {
			return StringPiece(&ptr_[start], len);
		};

		inline void advance(size_t len) {
			ptr_ += len;
		};

		size_t size() const {
			return len_;
		};

		string toString() const {
			return string(ptr_).substr(0, len_);
		};

		operator string() const {
			return toString();
		}
	};
};