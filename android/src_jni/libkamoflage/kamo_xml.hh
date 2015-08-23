/*
 * YAFFA, Yet Another F****** FTP Application
 * Copyright (C) 2005 by Mattias Eklöf
 * Copyright (C) 2008 by Anton Persson
 *
 * http://www.733kru.org/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* $Id: kamo_xml.hh,v 1.2 2009/03/31 12:25:23 pltxtra Exp $ */

#ifndef __KXMLDOC_HH
#define __KXMLDOC_HH

#include <string>
#include <vector>
#include <expat.h>
#include <sstream>
#include <iostream>

#include <jngldrum/jexception.hh>

/// from XML node k, get attribute a, store in b. Use value d as default on exception.
#define KXML_GET_NUMBER(k,a,b,d)			\
        try { \
		std::string strng = k.get_attr(a); \
		std::istringstream sin(strng); \
		sin >> b; \
	} catch(...) {b = d;}


class KXMLDoc {
	class Element {
	public:
		class Attribute {
		public:
			std::string name;
			std::string value;

			Attribute(const std::string &_name, const std::string &_value)
			{
				name = _name;
				value = _value;
			}
		};

		unsigned int ref_count;

		std::string name;
		unsigned short offset;

		std::vector<std::string> values;
		std::vector<std::vector<Element *> > children;
		std::vector<std::vector<Attribute *> > attributes;

		Element *parent;

		Element(const std::string &_name, std::vector<Attribute *> &_attributes, Element *_parent)
		{
			ref_count = 0;

			name = _name;
			attributes.push_back(_attributes);

			offset = 0;
			values.push_back(std::string());
			children.push_back(std::vector<Element *>());
			parent = _parent;
		}
		~Element(void);

		static void inc_ref_count(Element *e);
		static bool dec_ref_count(Element *e);

		std::string to_string(unsigned short _indent) const
		{
			std::ostringstream stream;
			for (int off = 0; off <= offset; off++) {
				for (int i = 0; i < _indent; i++)
					stream << "\t";

				stream << "<" << name;
				for (unsigned int i = 0; i < attributes[off].size(); i++)
					stream << " " << attributes[off][i]->name << "=\"" << attributes[off][i]->value << "\"";

				stream << ">";

				stream << values[off];

				for (unsigned int i = 0; i < children[off].size(); i++) {
					stream << std::endl;
					stream << children[off][i]->to_string(_indent+1);
					for (int j = 0; j < _indent; j++)
						stream << "\t";
				}

				stream << "</" << name << ">" << std::endl;
			}
			return stream.str();
		}

#ifdef DEBUG
		void print(unsigned short _indent) const
		{
			for (int off = 0; off <= offset; off++) {
				for (int i = 0; i < _indent; i++)
					std::cout << "\t";
				std::cout << "<" << name;
				for (int i = 0; i < attributes[off].size(); i++)
					std::cout << " " << attributes[off][i]->name << "=\"" << attributes[off][i]->value << "\"";
				std::cout << ">";

				std::cout << values[off];

				for (int i = 0; i < children[off].size(); i++) {
					std::cout << std::endl;
					children[off][i]->print(_indent+1);
					for (int i = 0; i < _indent; i++)
						std::cout << "\t";
				}

				std::cout << "</" << name << ">" << std::endl;
			}
		}
#endif
	};

	static void element_start(void *obj, const char *el, const char **attr);
	static void element_end(void *obj, const char *el);
	static void character_data(void *obj, const char *, int);

	Element *root_element;
	unsigned int element_sub_index;

protected:
	KXMLDoc(KXMLDoc::Element *);
	KXMLDoc(KXMLDoc::Element *, unsigned int);

public:
	template<class charT, class Traits>
	friend std::basic_istream<charT, Traits> &operator>>(std::basic_istream<charT, Traits> &, KXMLDoc &);

	KXMLDoc(void);
	KXMLDoc(const KXMLDoc &);
	~KXMLDoc(void);
	const std::string &get_name(void) const;
	const std::string &get_value(void) const;
	const std::string &get_attr(const std::string &) const;
	std::string to_string() const;
	unsigned int get_count(void) const;
	unsigned int get_count(const std::string &) const;
	KXMLDoc operator[](const std::string &) const;
	KXMLDoc operator[](unsigned int) const;
	KXMLDoc operator=(const KXMLDoc &);
#ifdef DEBUG
	void print(void) const;
#endif

	static std::string escaped_string(const std::string &input);

};



template<class charT, class Traits>
std::basic_istream<charT, Traits> &operator>>(std::basic_istream<charT, Traits> &is, KXMLDoc &xmlObj)
{
        if(!is.good()) return is;

	if(xmlObj.root_element != NULL) {
		KXMLDoc::Element::dec_ref_count(xmlObj.root_element);
		xmlObj.root_element = NULL;
	}

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, (void *)&xmlObj);
	XML_SetElementHandler(parser, KXMLDoc::element_start, KXMLDoc::element_end);
	XML_SetCharacterDataHandler(parser, KXMLDoc::character_data);

	std::string buffer;
	bool done = false;
	std::string emsg = "";

	do {
		std::getline(is, buffer);
		done = is.fail() | is.eof();
		if (!XML_Parse(parser, buffer.c_str(), buffer.length(), done) )
		{
			emsg = XML_ErrorString(XML_GetErrorCode(parser));
		}
	} while (!done);

	XML_ParserFree(parser);

	if (emsg.size() > 0 ) {
		xmlObj.root_element = NULL;
		throw jException(emsg, jException::sanity_error);
	}

        return is;
}

#endif
