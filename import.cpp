
// grindtrick import pstream
// grindtrick import boost_locale
// grindtrick import boost_filesystem
// grindtrick import boost_system

#include <algorithm>
#include <iostream>
#include <iterator>
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

wptree replace_nbsp(wptree const & n)
{
    auto d = n.data();
    replace_all(d, L"&nbsp;", L" ");

    auto r = wptree(d);

    for(auto child: n)
    {
        auto no_nbsp = replace_nbsp(child.second);
        r.push_back( make_pair(child.first, no_nbsp ) );
    }

    return r;
}

wstring to_html(wptree const & enote, wstring const & title)
{
    // nbsp are preserved by read_xml.
    // The data in the nodes have been decoded, except
    // nbsp.  If the original enote contained "&lt;",
    // data is "<", but "&nbsp;" are left as-is.

    // We do not care ofr nbspaces and we do not want 
    // the string "&nbsp;" to appear as regular text 
    // in our output.  So we remove them.

    auto copy = replace_nbsp(enote);

    wostringstream os;
    write_xml(os, copy);

    wstring encoded_title(title);
    if( encoded_title.size() )
    {
        replace_all(encoded_title, L"<", L"&lt;");
        replace_all(encoded_title, L">", L"&gt;");
        replace_all(encoded_title, L"&", L"&amp;");

        encoded_title = L"<title>" + encoded_title + L"</title>\n";
    }

    auto r = os.str();

    erase_all(r, L"<?xml version=\"1.0\" encoding=\"utf-8\"?>");

    r = L"<!DOCTYPE html>\n" 
        L"<html>\n"
        L"<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\">\n"
        + encoded_title +
        L"<body>\n"
        + r +
        L"</body></html>\n"
    ;

    return r;
}

wstring to_markdown(wstring const & content)
{
    using namespace redi;

    vector<string> args;
    args.push_back("html2text");
    args.push_back("-nobs");
    args.push_back("-style");
    args.push_back("pretty");
    args.push_back("-width");
    args.push_back("99");

    pstream in("html2text", args, pstreambuf::pstdin | pstreambuf::pstdout);

    string utf8 = utf_to_utf<char, wchar_t>(content.c_str());

    //std::ofstream ifs("to_markdown_input");
    //ifs << utf8;

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

    in.close();
    if( !in.rdbuf()->exited() )
    {
        throw check_error(L"html2text failed to exit");
    }

    if( in.rdbuf()->status() != 0 )
    {
        throw check_error(L"html2text exited with non-zero status");
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
    wstring content_as_html_;
    set<wstring> tags_;
    wstring project_;
    vector<Attachment> attachments_;
    wstring basename_;
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
    wstring invalids = L"<>:\"/\\|?*";
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

void repair_title(wstring & t)
{
    wstring r;
    copy_if(t.begin(), t.end(), back_inserter(r), is_valid_fn_char);
    t = r;
    trim(t);
}

void check_for_valid_title(wstring const & t)
{
    if( ! all_of(t.begin(), t.end(), is_valid_fn_char) ) throw check_error(L"title contains invalid characters");
}

void write_attachment(boost::filesystem::wpath const & annex_path, Attachment const & a, int & counter)
{
    auto afn = a.filename_;

    if( afn.empty() )
    {
        wostringstream afn_os;
        afn_os << setw(5) << setfill(L'0')  << counter;
        ++ counter;
        afn = afn_os.str();
    }

    boost::filesystem::wpath path = annex_path / afn;

    if( a.encoding_ != L"base64" )
    {
        throw check_error(L"unsupported attachment encoding: " + a.encoding_);
    }

    string data;
    data.resize(a.data_.size());

    /*
    Note: copy wchar_t down to char.  This is OK
    because the string is base64 encoded data, so
    no character is beyond 127.
    */
    copy(a.data_.begin(), a.data_.end(), data.begin());
    
    erase_all(data, "\n");
    erase_all(data, "\r");

    auto dec = b64decode(data);
    auto fn = path.string<string>();
    std::ofstream os(fn, ios::binary);
    os.exceptions(~ios::goodbit);
    os << dec;
}

void write_note(Note const & n)
{
    wstring wnote_fn = n.basename_ + L".md";
    string note_fn = from_utf(wnote_fn, "UTF-8") ;

    std::wofstream os(note_fn);

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
        boost::filesystem::wpath annex_fn(n.basename_);
        annex_fn += L".annexes";
        create_directory(annex_fn);
        int counter = 1;
        for(auto a: n.attachments_)
        {
            write_attachment(annex_fn, a, counter);
        }
    }
}

void write_html_content(Note const & n)
{
    wstring whtml_fn = n.basename_ + L".html";
    string html_fn = from_utf(whtml_fn, "UTF-8") ;

    std::wofstream os(html_fn);

    os << n.content_as_html_;
}

void print_tree(wptree const & t, int indent=0)
{
    wstring ide(indent*2, ' ');

    std::wcout << ide;
    std::wcout << t.data() << "\n";

    for(auto c: t)
    {
        std::wcout << ide;
        std::wcout << "<" << c.first << ">\n";

        print_tree(c.second, indent+1);
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
            repair_title(n.title_);
            check_for_valid_title(n.title_);
        }
        else if( v.first == L"content")
        {
            wstring en_note = v.second.get_value<wstring>();
            // en_note is an XML document until we get 
            // inside the <en-note> tag.  In there is HMTL.

            wptree en_note_ptree;
            wistringstream is(en_note);
            read_xml(is, en_note_ptree);

            auto enote = en_note_ptree.get_child(L"en-note");

            n.content_as_html_ = to_html(enote, n.title_);

            auto pre_opt = enote.get_child_optional(L"pre");
            if( pre_opt )
            {
                n.content_ = (*pre_opt).get_value<wstring>();
            }
            else
            {
                n.content_ = to_markdown( n.content_as_html_ );
            }
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

    n.basename_ = g_sphere_tag + L" " + n.project_ + L" " + n.title_;

    write_note(n);
    write_html_content(n);
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
