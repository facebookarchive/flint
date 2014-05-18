#pragma once

#include <iostream>
#include <string>
#include <array>
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
		ErrorObject(Lint type, size_t line, string title, string desc) :
			m_type(type), m_line(line), m_title(move(title)), m_desc(move(desc)) {};

		// Getter
		uint getType() const {
			return m_type;
		};

		/*
		* Prints an single error of the report in either
		* JSON or Pretty Printed format
		*
		*/
		void print(const string &path) const {

			static const array<string, 6> typeStr = {
				"[Error  ] ", "[Warning] ", "[Advice ] ", "Error", "Warning", "Advice"
			};

			if (Options.LEVEL < m_type) {
				return;
			}

			if (Options.JSON) {
				cout <<	"        {\n"
					"	        \"level\"    : \"" << typeStr[m_type + 3u]  << "\",\n"
					"	        \"line\"     : "   << to_string(m_line)     << ",\n"
					"	        \"title\"    : \"" << escapeString(m_title) << "\",\n"
					"	        \"desc\"     : \"" << escapeString(m_desc)  << "\"\n"
					"        }";

				return;
			}

			cout << typeStr[m_type] << path << ':' 
				 << to_string(m_line) << ": " << m_title << endl;
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

		explicit ErrorFile(string path) : ErrorBase(), m_path(move(path)) {};

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
			m_objs.push_back(move(error));
		};

		/*
		* Prints an single file of the report in either
		* JSON or Pretty Printed format
		*/
		void print() const {

			if (Options.JSON) {
				cout << "    {\n"
					"	    \"path\"     : \"" << escapeString(m_path)     << "\",\n"
					"	    \"errors\"   : "   << to_string(getErrors())   << ",\n"
					"	    \"warnings\" : "   << to_string(getWarnings()) << ",\n"
					"	    \"advice\"   : "   << to_string(getAdvice())   << ",\n"
					"	    \"reports\"  : [\n";

				for (size_t i = 0, size = m_objs.size(); i < size; ++i) {
					if (i > 0) {
						cout <<  ',' << endl;
					}

					m_objs[i].print(m_path);
				}

				cout << "\n      ]\n"
						"    }";

				return;
			}
			
			for (size_t i = 0, size = m_objs.size(); i < size; ++i) {
				m_objs[i].print(m_path);
			}
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

			m_files.push_back(move(file));
		};

		/*
		* Prints an entire report in either 
		* JSON or Pretty Printed format
		*
		* @return
		*		Returns a string containing the report output
		*/
		void print() const {
			
			if (Options.JSON) {
				cout << "{\n"
					"	\"errors\"   : " << to_string(getErrors())   << ",\n"
					"	\"warnings\" : " << to_string(getWarnings()) << ",\n"
					"	\"advice\"   : " << to_string(getAdvice())   << ",\n"
					"	\"files\"    : [\n";

				for (size_t i = 0, size = m_files.size(); i < size; ++i) {
					if (i > 0) {
						cout << ',' << endl;
					}

					m_files[i].print();
				}

				cout <<	"\n  ]\n"
						"}";

				return;
			}

			for (size_t i = 0, size = m_files.size(); i < size; ++i) {
				if (m_files[i].getTotal() > 0) {
					m_files[i].print();
				}
			}

			cout << "\nLint Summary: " << to_string(m_files.size()) << " files\n"
				"Errors: " << to_string(getErrors());

			if (Options.LEVEL >= Lint::WARNING) {
				cout << " Warnings: " << to_string(getWarnings());
			}
			if (Options.LEVEL >= Lint::ADVICE) {
				cout << " Advice: " << to_string(getAdvice());
			}
			cout << endl;
		};
	};

};
