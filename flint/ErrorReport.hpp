#pragma once

#include <string>
#include <vector>

#include "Polyfill.hpp"
#include "Options.hpp"

namespace flint {
	
#define TWIDTH 79

	class ErrorObject {
	private:
		// Members
		const Lint m_type;
		const uint m_line;
		const string m_title, m_desc;

	public:

		ErrorObject(Lint type, uint line, const string title, const string desc) :
			m_type(type), m_line(line), m_title(title), m_desc(desc) {};

		uint getType() const {
			return m_type;
		};

		string toString() const {

			const vector<string> m_typeStr = {
				"Error", "Warning", "Advice"
			};

			if (Options.JSON) {
				string result =
					"        {\n"
					"	        level    : " + m_typeStr[m_type] + ",\n"
					"	        line     : " + to_string(m_line) + ",\n"
					"	        title    : \"" + m_title + "\",\n"
					"	        desc     : \"" + m_desc + "\"\n"
					"        }";

				return result;
			}

			string result = "Line " + to_string(m_line) + ": "
				+ m_typeStr[m_type] + "\n\n"
				+ m_title + "\n\n";
				
			if (!m_desc.empty()) {
				result += m_desc + "\n\n";
			}

			return result;
		};
	};

	class ErrorBase {
	protected:
		// Members
		uint m_errors, m_warnings, m_advice;
	public:
		
		uint getErrors() const {
			return m_errors;
		};
		uint getWarnings() const {
			return m_warnings;
		};
		uint getAdvice() const {
			return m_advice;
		};
	};

	class ErrorFile : public ErrorBase {
	private:
		// Members
		vector<ErrorObject> m_objs;
		const string m_path;

	public:

		explicit ErrorFile(const string &path) : m_path(path) {};

		void addError(ErrorObject error) {
			if (error.getType() == Lint::ERROR) {
				++m_errors;
			}
			else if (error.getType() == Lint::WARNING) {
				++m_warnings;
			}
			if (error.getType() == Lint::ADVICE) {
				++m_advice;
			}
			m_objs.push_back(error);
		};

		string toString() const {

			if (Options.JSON) {
				string result =
					"    {\n"
					"	    path     : \"" + m_path + "\",\n"
					"	    errors   : " + to_string(getErrors()) + ",\n"
					"	    warnings : " + to_string(getWarnings()) + ",\n"
					"	    advice   : " + to_string(getAdvice()) + ",\n"
					"	    reports  : [\n";

				for (int i = 0; i < m_objs.size(); ++i) {
					if (i > 0) {
						result += ",\n";
					}

					result += m_objs[i].toString();
				}

				result +=
					"\n      ]\n"
					"    }";

				return result;
			}
			
			string divider = string(TWIDTH, '=');
			string result = divider + "\nFile " + m_path + ": \n"
				"Errors:   " + to_string(getErrors()) + "\n";
			if (Options.LEVEL >= Lint::WARNING) {
				result += "Warnings: " + to_string(getWarnings()) + "\n";
			}
			if (Options.LEVEL >= Lint::ADVICE) {
				result += "Advice:   " + to_string(getAdvice()) + "\n";
			}
			result += divider + "\n\n";

			for (int i = 0; i < m_objs.size(); ++i) {
				result += m_objs[i].toString();
			}

			return result;
		};
	};

	class ErrorReport : public ErrorBase {
	private:
		// Members
		vector<ErrorFile> m_files;
	public:

		void addFile(ErrorFile file) {
			m_errors	+= file.getErrors();
			m_warnings	+= file.getWarnings();
			m_advice	+= file.getAdvice();

			m_files.push_back(file);
		};

		string toString() const {
			
			if (Options.JSON) {
				string result =
					"{\n"
					"	errors   : " + to_string(getErrors()) + ",\n"
					"	warnings : " + to_string(getWarnings()) + ",\n"
					"	advice   : " + to_string(getAdvice()) + ",\n"
					"	files    : [\n";

				for (int i = 0; i < m_files.size(); ++i) {
					if (i > 0) {
						result += ",\n";
					}

					result += m_files[i].toString();
				}

				result +=
					"\n  ]\n"
					"}";

				return result;
			}

			string divider = string(TWIDTH, '=');
			string result = "";

			for (int i = 0; i < m_files.size(); ++i) {
				result += m_files[i].toString();
			}

			result += divider + "\nLint Summary: " 
				+ to_string(m_files.size()) + " files\n\n"
				"Errors:   " + to_string(getErrors()) + "\n";
			if (Options.LEVEL >= Lint::WARNING) {
				result += "Warnings: " + to_string(getWarnings()) + "\n";
			}
			if (Options.LEVEL >= Lint::ADVICE) {
				result += "Advice:   " + to_string(getAdvice()) + "\n";
			}
			result += divider + "\n";

			return result;
		};
	};

#undef TWIDTH

};