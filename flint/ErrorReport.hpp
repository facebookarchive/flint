#pragma once

#include <string>
#include <vector>

#include "Polyfill.hpp"
#include "Options.hpp"

namespace flint {

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
		string toString(const string &path) const {

			const vector<string> typeStr = {
				"Error  ", "Warning", "Advice "
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

			if (Options.LEVEL < m_type) {
				return "";
			}

			string result = "[" + typeStr[m_type] + "] " + path + ":" 
				+ to_string(m_line) + ": " + m_title + "\n";

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
		
		ErrorBase() : m_errors(0), m_warnings(0), m_advice(0) {};

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

		explicit ErrorFile(const string &path) : ErrorBase(), m_path(path) {};

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

				for (size_t i = 0; i < m_objs.size(); ++i) {
					if (i > 0) {
						result += ",\n";
					}

					result += m_objs[i].toString(m_path);
				}

				result +=
					"\n      ]\n"
					"    }";

				return result;
			}
			
			string result = "";
			for (size_t i = 0; i < m_objs.size(); ++i) {
				result += m_objs[i].toString(m_path);
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

		ErrorReport() : ErrorBase() {};

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

				for (size_t i = 0; i < m_files.size(); ++i) {
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

			string result = "";
			for (size_t i = 0; i < m_files.size(); ++i) {
				if (m_files[i].getTotal() > 0) {
					result += m_files[i].toString();
				}
			}

			result += "\nLint Summary: " 
				+ to_string(m_files.size()) + " files\n"
				"Errors: " + to_string(getErrors()) + " ";
			if (Options.LEVEL >= Lint::WARNING) {
				result += "Warnings: " + to_string(getWarnings()) + " ";
			}
			if (Options.LEVEL >= Lint::ADVICE) {
				result += "Advice: " + to_string(getAdvice()) + " ";
			}
			result += "\n";

			return result;
		};
	};

};