
// grindtrick import pstream
// grindtrick import boost_locale

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <boost/range/algorithm/count_if.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/locale/encoding.hpp>

#include <pstream.h>

using namespace boost::property_tree;
using namespace boost::locale::conv;
using namespace boost::algorithm;
using namespace boost::range;
using namespace std;

wstring g_sphere_tag;

wstring const SUBJECT_FIELD_NAME(L"Sujet");
wstring const TAG_FIELD_NAME(L"\u00C9tiquettes");

class check_error
{
public:
	check_error(wstring const & msg) : msg_(msg) {}

	void set_note_title(wstring const & t)
	{
		title_ = t;
	}

	wstring msg_;
	wstring title_;

	wstring user_message() const
	{
		if( title_.empty() ) return L"check_error: " + msg_;
		return L'"' + title_ + L"\": " + msg_;
	}
};

wstring to_markdown(wstring const & content)
{
	//wofstream ifs("to_markdown_input");
	//ifs << content;

	using namespace redi;

	vector<string> args;
	args.push_back("node");
	args.push_back("to_markdown.js");

	pstream in("node", args, pstreambuf::pstdin | pstreambuf::pstdout);

	string utf8 = utf_to_utf<char, wchar_t>(content.c_str());

	in << utf8;

	peof(in); // close stdin so client knows to finish

	//ofstream of("to_markdown_result");

	string line;
	string markdown;
	while( getline(in, line) )
	{
		markdown = markdown + line + "\n";
		//of << line << '\n';
	}	

	auto r = utf_to_utf<wchar_t, char>(markdown.c_str());

	replace_all(r, L"&nbsp;", L" ");

	return r;
}

class Attachment
{
public:
	wstring data_;
	wstring encoding_;
	wstring filename_;
};

class Note
{
public:
	wstring title_;
	wstring content_;
	set<wstring> tags_;
	wstring project_;
	vector<Attachment> attachments_;
};

Attachment resource(wptree const & pt)
{
	Attachment a;

	auto data_node = pt.get_child(L"data");

	a.data_ = data_node.get_value<wstring>();

	for(auto v: data_node)
	{
		if( v.first == L"<xmlattr>" )
		{
			for(auto v: v.second)
			{
				if( v.first == L"encoding" )
				{
					a.encoding_ = v.second.get_value<wstring>();
				}
			}
		}
	}

	auto opt_res_attr = pt.get_child_optional(L"resource-attributes");
	if( opt_res_attr )
	{
		auto res_attr = *opt_res_attr;
		auto opt_fn = res_attr.get_optional<wstring>(L"file-name");
		if( opt_fn ) a.filename_ = *opt_fn;
	}

	return a;
}

void check_for_valid_tag(wstring const & t)
{
	if( t.empty() ) throw check_error(L"tag cannot be empty string");
	if( count( t.begin(), t.end(), L'#' ) != 0 ) throw check_error(L"Evernote tag cannot contain hash");
	if( count_if( t, iswspace ) ) throw check_error(L"tag must be a single word");
}

void write_note(Note const & n)
{
	for(auto t: n.tags_)
	{
		check_for_valid_tag(t);
	}

	// TODO: make sure file name is clean for fielsystem

	wstring wfn = g_sphere_tag + L" " + n.project_ + L" " + n.title_ + L".md";
	string fn = from_utf(wfn, "UTF-8") ;

	wofstream os(fn);

	os << SUBJECT_FIELD_NAME << ": " << n.title_ << '\n';

	os << TAG_FIELD_NAME << ": ";

	for(auto t: n.tags_)
	{
		os << L'#' << t << L' ';
	}
	os << '\n';

	os << '\n';

	os << n.content_;

	// TODO: save attachments
}

void note(wptree const & pt)
{
	Note n;

	for(auto v: pt)
	{
		if( v.first == L"title" )
		{
			n.title_ = v.second.get_value<wstring>();
		}
		else if( v.first == L"content")
		{
			wstring en_note = v.second.get_value<wstring>();
			// en_note is a full XML document.
			// The HTML of the note is inside the <en-note> tag.

			wptree en_note_ptree;
			wistringstream is(en_note);
			read_xml(is, en_note_ptree);

			auto enote = en_note_ptree.get_child(L"en-note");

			wostringstream os;
			write_xml(os, enote);
			
			n.content_ = to_markdown( os.str() );
		}
		else if( v.first == L"tag")
		{
			n.tags_.insert( v.second.get_value<wstring>() );
		}
		else if( v.first == L"resource")
		{
			n.attachments_.push_back( resource(v.second) );
		}
	}

	n.tags_.insert(g_sphere_tag);

	n.project_ = L"imported";
	for(auto v: n.tags_)
	{
		if( v != g_sphere_tag )
		{
			n.project_ = v;
			break;
		}
	}

	n.tags_.insert(L"imported");

	try
	{
		write_note(n);
	}
	catch(check_error & ex)
	{
		ex.set_note_title(n.title_);
		throw;
	}
}

int main(int argc, char ** argv)
{
	// Load the system locale.
	setlocale(LC_ALL, "");


	try
	{
		wptree pt;

		char const * fn = "INRO.enex";
		g_sphere_tag = L"inro";

		if( argc >= 2 ) fn = argv[1];

		wifstream enex_file(fn);
		read_xml(enex_file, pt);

		for(auto v: pt.get_child(L"en-export"))
		{
			if( v.first == L"note" )
			{
				note(v.second);
			}
		}
	}	
	catch(exception const & ex)
	{
		cerr << "exception: " << ex.what() << '\n';
		return 1;
	}
	catch(check_error const & ex)
	{
		wcerr << ex.user_message() << '\n';
		return 1;
	}
	
	return 0;
}
