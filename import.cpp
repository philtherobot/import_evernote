
// grindtrick import pstream
// grindtrick import boost_locale
// grindtrick import boost_filesystem
// grindtrick import boost_system

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <set>
#include <boost/range/algorithm/count_if.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/locale/encoding.hpp>
#include <boost/filesystem.hpp>

#include <pstream.h>

using namespace boost::property_tree;
using namespace boost::locale::conv;
using namespace boost::algorithm;
using namespace boost::range;
using namespace boost::filesystem;
using namespace std;

#include "base64.cpp"

wstring g_sphere_tag;

wstring const SUBJECT_FIELD_NAME(L"Sujet");
wstring const TAG_FIELD_NAME(L"\u00C9tiquettes");

class check_error
{
public:
	check_error(wstring const & msg) : msg_(msg) {}

	wstring msg_;

	wstring user_message() const
	{
		return msg_;
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

bool is_valid_fn_char(wchar_t c)
{
	if( c < 32 ) return false;
	wstring invalids = L"<>:\"/\\|?*.";
	if( count(invalids.begin(), invalids.end(), c) != 0 ) return false;
	return true;
}

void check_for_valid_tag(wstring const & t)
{
	if( t.empty() ) throw check_error(L"tag cannot be empty string");
	if( count( t.begin(), t.end(), L'#' ) != 0 ) throw check_error(L"Evernote tag cannot contain hash");
	if( count_if( t, iswspace ) ) throw check_error(L"tag must be a single word");
	if( ! all_of(t.begin(), t.end(), is_valid_fn_char) ) throw check_error(L"tag contains invalid characters");
}

void check_for_valid_title(wstring const & t)
{
	if( ! all_of(t.begin(), t.end(), is_valid_fn_char) ) throw check_error(L"title contains invalid characters");
}

void write_attachment(boost::filesystem::wpath const & annex_path, Attachment const & a)
{
	boost::filesystem::wpath path = annex_path / a.filename_;

	if( a.encoding_ != L"base64" )
	{
		throw check_error(L"unsupported attachment encoding: " + a.encoding_);
	}

	string data;
	data.resize(a.data_.size());
	copy(a.data_.begin(), a.data_.end(), data.begin());
	erase_all(data, "\n");
	erase_all(data, "\r");

	auto dec = b64decode(data);
	cout << "data size = " << dec.size() << '\n';
	auto fn = path.string<string>();
	cout << "file is " << fn << '\n';
	std::ofstream os(fn, ios::binary);
	os.exceptions(~ios::goodbit);
	os << dec;
}

void write_note(Note const & n)
{
	wstring wfn = g_sphere_tag + L" " + n.project_ + L" " + n.title_ + L".md";
	string fn = from_utf(wfn, "UTF-8") ;

	std::wofstream os(fn);

	os << SUBJECT_FIELD_NAME << ": " << n.title_ << '\n';

	os << TAG_FIELD_NAME << ": ";

	for(auto t: n.tags_)
	{
		os << L'#' << t << L' ';
	}
	os << '\n';

	os << '\n';

	os << n.content_;

	if( !n.attachments_.empty() )
	{
		boost::filesystem::wpath note_fn(wfn);
		boost::filesystem::wpath annex_fn = note_fn.stem().wstring() + L".annexes";
		create_directory(annex_fn);
		for(auto a: n.attachments_)
		{
			write_attachment(annex_fn, a);
		}
	}
}

void note(wptree const & pt)
{
	Note n;

	for(auto v: pt)
	{
		if( v.first == L"title" )
		{
			n.title_ = v.second.get_value<wstring>();
			trim(n.title_);
			check_for_valid_title(n.title_);
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
			auto tag = v.second.get_value<wstring>();
			trim(tag);
			check_for_valid_tag(tag);
			n.tags_.insert(tag);
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

	write_note(n);
}

int main(int argc, char ** argv)
{
	// Load the system locale.
	setlocale(LC_ALL, "");


	try
	{
		g_sphere_tag = L"inro";

		if( argc >= 2 ) 
		{
			g_sphere_tag = to_utf<wchar_t>(argv[1], "UTF-8");
		}

		regex wc("[0-9]*\\.xml");
		smatch mo;

		for(auto e: directory_iterator("."))
		{
			if( !is_regular_file(e) ) continue;

			string fn = e.path().filename().string();

			if( !regex_match(fn, mo, wc) ) continue;

			std::wifstream note_file(e.path().string());
			note_file.exceptions(~ios::goodbit);

			wptree pt;
			read_xml(note_file, pt);

			cout << fn << '\n';

			try
			{
				note(pt);
				remove(e.path());
			}
			catch(exception const & ex)
			{
				cerr << "exception: " << ex.what() << '\n';
			}
			catch(check_error const & ex)
			{
				wcerr << fn << ": " << ex.user_message() << '\n';
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
