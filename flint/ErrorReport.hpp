#pragma once

#include <string>
#include <vector>

#include "Polyfill.hpp"
#include "Options.hpp"

namespace flint {

// Standard terminal width for divider
#define TWIDTH 79

	/*
	* Class to represent a single "Error" that was found during linting
	*/
	class ErrorObject {
	private:
		// Members
		const Lint m_type;
		const size_t m_line;
		const string m_title, m_desc;

	public:

		// Constructor
		ErrorObject(Lint type, size_t line, const string title, const string desc) :
			m_type(type), m_line(line), m_title(title), m_desc(desc) {};

		// Getter
		uint getType() const {
			return m_type;
		};

		/*
		* Prints an single error of the report in either
		* JSON or Pretty Printed format
		*
		* @return
		*		Returns a string containing the report output
		*/
		string toString() const {

			const vector<string> typeStr = {
				"Error", "Warning", "Advice"
			};

			if (Options.JSON) {
				string result =
					"        {\n"
					"	        \"level\"    : \"" + typeStr[m_type] + "\",\n"
					"	        \"line\"     : " + to_string(m_line) + ",\n"
					"	        \"title\"    : \"" + escapeString(m_title) + "\",\n"
					"	        \"desc\"     : \"" + escapeString(m_desc) + "\"\n"
					"        }";

				return result;
			}

			string result = "Line " + to_string(m_line) + ": "
				+ typeStr[m_type] + "\n\n"
				+ m_title + "\n\n";
				
			if (!m_desc.empty()) {
				result += m_desc + "\n\n";
			}

			return result;
		};
	};

	/*
	* Base Class for ErrorFile and ErrorReport which both have error counts
	*/
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
		uint getTotal() const {
			return m_advice + m_warnings + m_errors;
		};
	};

	/*
	* Class to represent a single file's "Errors" that were found during linting
	*/
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

		/*
		* Prints an single file of the report in either
		* JSON or Pretty Printed format
		*
		* @return
		*		Returns a string containing the report output
		*/
		string toString() const {

			if (Options.JSON) {
				string result =
					"    {\n"
					"	    \"path\"     : \"" + escapeString(m_path) + "\",\n"
					"	    \"errors\"   : " + to_string(getErrors()) + ",\n"
					"	    \"warnings\" : " + to_string(getWarnings()) + ",\n"
					"	    \"advice\"   : " + to_string(getAdvice()) + ",\n"
					"	    \"reports\"  : [\n";

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

	/*
	* Class to represent the whole report and all "Errors" that were found during linting
	*/
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

		/*
		* Prints an entire report in either 
		* JSON or Pretty Printed format
		*
		* @return
		*		Returns a string containing the report output
		*/
		string toString() const {
			
			if (Options.JSON) {
				string result =
					"{\n"
					"	\"errors\"   : " + to_string(getErrors()) + ",\n"
					"	\"warnings\" : " + to_string(getWarnings()) + ",\n"
					"	\"advice\"   : " + to_string(getAdvice()) + ",\n"
					"	\"files\"    : [\n";

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
				if (m_files[i].getTotal() > 0) {
					result += m_files[i].toString();
				}
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

// Cleanup
#undef TWIDTH

};