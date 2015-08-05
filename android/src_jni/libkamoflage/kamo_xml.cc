/*
 * YAFFA, Yet Another F****** FTP Application
 * Copyright (C) 2005 by Mattias Eklöf
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

/* $Id: kamo_xml.cc,v 1.3 2009/03/31 12:25:23 pltxtra Exp $ */

#include "kamo_xml.hh"

#ifdef DEBUG
#include <assert.h>
#endif
#include <iostream>

void KXMLDoc::Element::inc_ref_count(Element *e)
{
#ifdef DEBUG
	assert(e->ref_count+1 > e->ref_count);
#endif
	e->ref_count++;

	for (unsigned int i = 0; i < e->children.size(); i++) {
		for (unsigned int j = 0; j < e->children[i].size(); j++)
			Element::inc_ref_count(e->children[i][j]);
	}
}

bool KXMLDoc::Element::dec_ref_count(Element *e)
{
	bool retval = false;
	if(e == NULL) return retval;
#ifdef DEBUG
	assert(e->ref_count-1 < e->ref_count);
#endif
	e->ref_count--;
	for (unsigned int i = 0; i < e->children.size(); i++) {
		std::vector<Element *>::iterator k;

		for(k = e->children[i].begin(); k != e->children[i].end();) {
			if(Element::dec_ref_count(*k)) {
				e->children[i].erase(k);
			} else k++;
		}
	}
	if (e->ref_count == 0) {
		retval = true;
		delete e;
	}
	return retval;
}

void KXMLDoc::element_start(void *obj, const char *el, const char **attr)
{
	std::vector<KXMLDoc::Element::Attribute *> attrvec;
	int i = 0;
	while (attr[i] != NULL) {
		attrvec.push_back(new Element::Attribute(attr[i], attr[i+1]));
		i += 2;
	}
	
	if ((*(KXMLDoc *)obj).root_element == NULL) {
		Element *e = new Element(el, attrvec, NULL);
		Element::inc_ref_count(e);		
		(*(KXMLDoc *)obj).root_element = e;
	} else {
		for (unsigned int i = 0;
		     i < (*(KXMLDoc *)obj).root_element->children[(*(KXMLDoc *)obj).root_element->offset].size();
		     i++) {
			if ((*(KXMLDoc *)obj).root_element->children[(*(KXMLDoc *)obj).root_element->offset][i]->name == el) {
				(*(KXMLDoc *)obj).root_element->children[(*(KXMLDoc *)obj).root_element->offset][i]->offset++;
				(*(KXMLDoc *)obj).root_element = (*(KXMLDoc *)obj).root_element->children[(*(KXMLDoc *)obj).root_element->offset][i];

				(*(KXMLDoc *)obj).root_element->values.push_back(std::string());
				(*(KXMLDoc *)obj).root_element->children.push_back(std::vector<Element *>());
				(*(KXMLDoc *)obj).root_element->attributes.push_back(attrvec);
				
				return;
			}
		}

		Element *e = new Element(el, attrvec, (*(KXMLDoc *)obj).root_element);
		Element::inc_ref_count(e);
		
		((*(KXMLDoc *)obj).root_element->children[(*(KXMLDoc *)obj).root_element->offset]).push_back(e);
		(*(KXMLDoc *)obj).root_element = e;
	}
}

void KXMLDoc::element_end(void *obj, const char *el)
{
#ifdef DEBUG
	assert((*(KXMLDoc *)obj).root_element != NULL);
#endif 

	if ((*(KXMLDoc *)obj).root_element->parent != NULL)
		(*(KXMLDoc *)obj).root_element = (*(KXMLDoc *)obj).root_element->parent;
}

void KXMLDoc::character_data(void *obj, const char *_str, int _len)
{
	(*(KXMLDoc *)obj).root_element->values[(*(KXMLDoc *)obj).root_element->offset] = std::string(_str, _len);
}

KXMLDoc::KXMLDoc(KXMLDoc::Element *el)
{
	root_element = el;
	Element::inc_ref_count(root_element);
	
	element_sub_index = 0;
}

KXMLDoc::KXMLDoc(KXMLDoc::Element *el, unsigned int _idx)
{
	root_element = el;
	Element::inc_ref_count(root_element);
	
	element_sub_index = _idx;
}

KXMLDoc::KXMLDoc(void)
{
	root_element = NULL;
	element_sub_index = 0;
}

KXMLDoc::KXMLDoc(const KXMLDoc &_xmlobj)
{
	root_element = _xmlobj.root_element;
	Element::inc_ref_count(root_element);
	
	element_sub_index = _xmlobj.element_sub_index;
}

KXMLDoc::~KXMLDoc(void)
{
	if (root_element != NULL) {
		(void) Element::dec_ref_count(root_element);
                root_element = NULL;
	}
}

const std::string &KXMLDoc::get_name(void) const
{
	return root_element->name;
}

const std::string &KXMLDoc::get_value(void) const
{
	return root_element->values[element_sub_index];
}

const std::string &KXMLDoc::get_attr(const std::string &_name) const
{
	for (unsigned int i = 0; i < root_element->attributes[element_sub_index].size(); i++) {
		if (root_element->attributes[element_sub_index][i]->name == _name)
			return root_element->attributes[element_sub_index][i]->value;
	}

	std::cout << "    No such attribute ]" << _name << "[.\n";
	throw jException("No such attribute", jException::sanity_error);
}

std::string KXMLDoc::to_string(void) const
{
	return root_element->to_string(0);
}

unsigned int KXMLDoc::get_count(void) const
{
	//if (element_sub_index != 0) {
	//	return 0;
	//} // XXX should I throw an exception or not?
	// throw jException("Sub index error", jException::sanity_error);
	
	return root_element->offset+1;
}

unsigned int KXMLDoc::get_count(const std::string &_name) const
{
      try {
              return (*this)[_name].get_count();
      } catch(...) {
              return 0;
      }
}

KXMLDoc KXMLDoc::operator[](const std::string &_name) const
{
//	if (element_sub_index != 0)
//		throw jException("Sub index error", jException::sanity_error);
	
	for (unsigned int i = 0; i < root_element->children[element_sub_index].size(); i++) {
		if (root_element->children[element_sub_index][i]->name == _name) {
			return KXMLDoc(root_element->children[element_sub_index][i]);
		}
	}
	
	std::cout << "    No such element ]" << _name << "[.\n";
	throw jException(std::string("No such element \"") + _name + "\" in XML-tree", jException::sanity_error);
}

KXMLDoc KXMLDoc::operator[](unsigned int _idx) const
{
	if (_idx > root_element->offset) {
		std::cout << "    Index out of bounds.\n";
		throw jException("Index out of bounds", jException::sanity_error);
	}
	
	return KXMLDoc(root_element, _idx);
}
KXMLDoc KXMLDoc::operator=(const KXMLDoc &_xmlobj)
{
      root_element = _xmlobj.root_element;
      Element::inc_ref_count(root_element);

      element_sub_index = _xmlobj.element_sub_index;

      return *this;
}

#ifdef DEBUG
void KXMLDoc::print(void) const
{
	root_element->print(0);
}
#endif

std::string KXMLDoc::escaped_string(const std::string &input) {
	unsigned int k;
	std::string output;
	for(k = 0; k < input.size(); k++) {
		switch(input[k]) {
		case '&':
			output += "&amp;";
			break;
		case '<':
			output += "&lt;";
			break;
		case '>':
			output += "&gt;";
			break;
		default:
			output += input[k];
			break;
		}
	}
	return output;
}

