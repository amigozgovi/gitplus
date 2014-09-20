#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <dirent.h>
#include <sha1.h>
#include <sha1.cpp>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <zlib.h>
#include <stdexcept>
#include <cstring>

using namespace std;

class Directory
{
    public:
        map<string,string> file_list;
        map<string,Directory> directory_list;
};

class GitCommit
{
    public:
        string tree;
        string parent;
        string author;
        string committer;
        string message;
        int size;
};

class GitBlob
{
    public:
        string name;
        string rev;
        string hash_key;
        int size;
};

class GitTree
{
    public:
        string rev;
        string name;
        string hash_key;
        map<string,GitBlob> blob_list;
        map<string,GitTree> tree_list;
        int size;
};

class Commit
{
    public:
        string rev;         //Revision
        string date;        //Date of commit
        string author;      //Author
        string state;       //State of commit
        string branches;    //Branches if any
        string next;        //Next revision
};

class Log
{
    public:
        string rev;         //Revision
        string msg;         //Commit Message
        string content;     //Commit Data
};

class Header
{
    public:
        string tags;    //Holds the tags in the cvs file
        string lastRev; //The last revision made
};

class Revision
{
    public:
        string rev;
        string date;
        string author;
        string state;
        string branches;
        string next;
        string msg;
        string content;
};

class Data
{
    public:
        string tags;
        string header;
        map<string, Revision> r;
};

vector<int> pos_list;           //Global variable to store positions { used in find_pos() }
vector<int>::iterator it;       //Global variable to hold iterator for pos_list

Data parse(string &);            //Parses a CVS Pattern and convert into normal form
string get_file(string &);         //Reads a file to a string  { utility function }

Header parseHeader(string &);                           //Parse Header part of the cvs pattern { called from parse() }
map<string,Commit> parseCommitInfo(string &);           //Parse Commit part of the cvs pattern { called from parse() }
map<string,Log> parseLog(string &);                     //Parse Log Part of the cvs pattern { called from parse() }

void split_string(vector<string> &result, const string &main, const string &search, const string &sub);         // Splits a string into multiple strings based on a token ( search )
vector<int> find_pos(const string &, const string &, const string &, int pos=0);                                // Finds positions of a substring in a string
string get_string_between( const string &main, const string &start, const string &end, int pos=0);              // Returns a substring between a start string and ending string
void remove_line(vector<string> &main, int &pos, int &no_of_lines);                                             // Removes a line from a vector of strings
void add_line(vector<string> &main, string &value, int &pos);                                                   // Adds a line to a vector of strings

string get_hash(string &);
string make_blob(string &);
string make_tree(GitTree gt);
int write_object( string &content,string &hash_key);

Directory get_dir_listing(string path)
{
    Directory dir;
    DIR *pdir,*pdir_2;
    struct dirent *pent;

    pdir=opendir(path.c_str());
    if(!pdir)
    {
        return dir;
    }

    string name,path_2;

    while(pent=readdir(pdir))
    {
        name=pent->d_name;
        path_2=path+"\\"+name;
        if(name=="." || name=="..")
            continue;

        if(opendir(path_2.c_str()))
        {
            Directory d2=get_dir_listing(path_2);
            dir.directory_list.insert(pair<string,Directory>(name,d2));
        }
        else
        {
            if(name.substr(name.size()-2,2)==",v" || name.substr(name.size()-2,2)==",V")
                dir.file_list.insert(pair<string,string>(name,path_2));
        }
    }
    closedir(pdir);
    return dir;
}

string make_tree(GitTree gt)
{
    stringstream tree_content,temp;
    tree_content<<"tree ";
    string hash_key,content;

    if(gt.blob_list.size()<1 && gt.blob_list.size()<1)
    {
        //cout<<"NO BLOB / TREE 4 "<<gt.name<<endl;
        return "";
    }
    if(gt.blob_list.size()>0)
    {
        for(map<string,GitBlob>::iterator gb_it=gt.blob_list.begin(); gb_it!=gt.blob_list.end(); ++gb_it)
        {
            temp<<"10644 blob "<<(*gb_it).second.hash_key<<"\t"<<(*gb_it).second.name<<"\n";
        }
    }
    if(gt.tree_list.size()>0)
    {
        for(map<string,GitTree>::iterator gt_it=gt.tree_list.begin(); gt_it!=gt.tree_list.end(); ++gt_it)
        {
            temp<<"040000 tree "<<(*gt_it).second.hash_key<<"\t"<<(*gt_it).second.name<<"\n";
        }
    }
    content=temp.str();
    tree_content<<content.size()<<"\0"<<content;
    content=tree_content.str();
    hash_key=get_hash(content);

    int ret=write_object(content,hash_key);
    if(ret>0)
        return hash_key;
    else
        cout<<"FAILED TREE"<<endl;
    return "";

}

int write_object( string &content,string &hash_key)
{
    DIR *dir;
    dir=opendir("objects");
    if(!dir)
    {
        mkdir("objects");
    }
    string dir_name="objects\\"+hash_key.substr(0,2);
    string file_name=dir_name+"\\"+hash_key.substr(2,hash_key.size());
    int re=mkdir(dir_name.c_str());

    if(!re)
    {
        dir=opendir(dir_name.c_str());
        if(!dir)
        {
            cout<<"FAILED TO MAKE DIRECTORY"<<endl;
            return -1;
        }
    }

    ofstream fout(file_name.c_str());
    if(!fout.good())
    {
        return 0;
    }
    fout<<content;
    fout.close();
    return 1;
}

GitTree loop_dir_list(Directory dir)
{
    string content,blob_hash,tree_hash;
    Data d;
    GitTree gt;
    GitBlob gb;

    if(dir.directory_list.size()>0)
    {
        for(map<string,Directory>::iterator it=dir.directory_list.begin(); it!=dir.directory_list.end(); ++it)
        {
            gt.name=(*it).first;
            GitTree gt_2=loop_dir_list((*it).second);
            gt.tree_list.insert(pair<string,GitTree>(gt.name,gt_2));

            cout<<"TREE : "<<gt.name<<endl<<endl;
        }
    }
    if(dir.file_list.size()>0)
    {
        for(map<string,string>::iterator it_3=dir.file_list.begin(); it_3!=dir.file_list.end(); ++it_3)
        {
            content=get_file((*it_3).second);
            d=parse(content);

            for(map<string,Revision>::iterator rev_it=d.r.begin(); rev_it!=d.r.end(); ++rev_it)
            {
                blob_hash=make_blob((*rev_it).second.content);

                cout<<"BLOB HASH: "<<blob_hash<<endl;

                if(!blob_hash.empty())
                {
                    gb.rev=(*rev_it).second.rev;
                    gb.name=(*it_3).first;
                    gb.hash_key=blob_hash;
                    gt.blob_list.insert(pair<string,GitBlob>(gb.rev,gb));

                    //Add revision no. to each tree object, so that later we can recognize, which tree is available in a revision for a particular file.
                    for(map<string,GitTree>::iterator gt_it=gt.tree_list.begin();gt_it!=gt.tree_list.end();++gt_it)
                    {
                        (*gt_it).second.rev=gb.rev;
                    }
                }
            }
        }
    }
    tree_hash=make_tree(gt);
    gt.hash_key=tree_hash;
    return gt;
}

string make_blob(string &content)
{
    stringstream ss;
    if(!content.empty())
    {
        int size=content.size();
        string blob_content;

        ss<<"blob "<<size<<"\0"<<content;
        blob_content=ss.str();
        string hash_key=get_hash(blob_content);
        int ret=write_object(blob_content,hash_key);
        if(ret>0)
            return hash_key;
        else
            cout<<"FAILED BLOB"<<endl;
    }
    return "";
}

string get_hash(string &content)
{
    SHA1 sha;

    string hash;

    unsigned message_digest[5];
    stringstream ss;
    sha.Reset();
    sha << content.c_str();
    if(!sha.Result(message_digest))
    {
        cout<<"SHA1 KEY NOT RECIEVED"<<endl;
    }
    else
    {
        for(int i = 0; i < 5 ; i++)
        {
            ss.setf(ios::hex|ios::uppercase,ios::basefield);
            ss.setf(ios::uppercase);
            ss<<message_digest[i];
        }
        hash.append(ss.str()+" ");
        return hash;
    }
}

int main(int argc, char** argv)
{
    Directory dir;
    dir=get_dir_listing(argv[1]);
    GitTree gt;
    gt=loop_dir_list(dir);
}

Data parse(string &data)
{
    /*
        parse the cvs pattern into normal form.
        Content of cvs file will be received as an argument in string data

        splits the content into 3 parts: header, commit info, log

        calls 3 sub functions parseHeader(), parseCommitInfo(), parseLog() for the purpose of converting into normal form and last all these parts will be joined together
    */
    string header,commitInfo,log;       //holds header, commit, log part respectively

    header=get_string_between(data,"","\n\n");      // header begins from start of file and ends at first 2 newlines
    commitInfo=get_string_between(data,"\n\n\n","\n\ndesc");    //commit info starts from 3 new lines and ends at 2 new line desc
    log=get_string_between(data,"@@\n\n\n","");     //log starts at @@ 3 ne lines and continues to the end of the file
    try
    {
        Header h;                   //To hold header info
        map<string,Commit> c;       // To hold commit info
        map<string, Log> l;         // To Hold log info
        string file_name;           // File name to use for writing contents to file
        Data d;
        map<string, Revision> r;
        Revision re;

        h=parseHeader(header);      // Extracts header part from the pattern
        c=parseCommitInfo(commitInfo);  //Extracts commit part from the pattern
        l=parseLog(log);                // Extracts log part from the pattern



        if(l.size()<1 || c.size()<1)
            return d;
        d.header=h.lastRev;
        d.tags=h.tags;
        for(map<string,Commit>::iterator it=c.begin();it!=c.end();++it)         // Beginning from the 1st revision till the end, we traverse through each element
        {
            if(l.count((*it).first)>0)
            {
                map<string, Log>::iterator it_2=l.find((*it).first);           // Search for the revision inside log so that we can merge it with the commit info
                re.rev=(*it).first;
                re.date=(*it).second.date;
                re.author=(*it).second.author;
                re.state=(*it).second.state;
                re.branches=(*it).second.branches;
                re.next=(*it).second.next;
                re.msg=(*it_2).second.msg;
                re.content=(*it_2).second.content;
                r.insert(pair<string,Revision>(re.rev,re));
            }
        }
        d.r=r;
        return d;
    }
    catch(exception &e)
    {
        cout<<"EXCEPTION RAISED: "<<e.what()<<endl;
    }
}

map<string,Commit> parseCommitInfo(string &commitInfo)
{
    string temp;
    Commit a;
    vector<string> SplitCommit;
    int i=0;
    map<string, Commit> m1;                                     //Will hold the info. for each revision

    split_string(SplitCommit,commitInfo,"next","\n\n");         //Splits the commit part to seperate revision numbers, the pattern is that each revision ends after the 2 new line characters following a "next"  string
    for(i=0;i<SplitCommit.size();++i)
    {
        temp=SplitCommit.at(i);
        a.rev=get_string_between(temp,"","\n");                 //Get the revision number
        a.date=get_string_between(temp,"date\t",";\t");         //Get the date of revision
        a.author=get_string_between(temp,"author ",";\t");      //Author info.
        a.state=get_string_between(temp,"state ",";\n");        //State: dead means the file was removed
        a.branches=get_string_between(temp,"branches",";\n");   //Branches of revision
        a.next=get_string_between(temp,"next\t",";\n");         //Next revision
        m1.insert(pair<string,Commit>(a.rev,a));                //Insert this info. to the map
    }

    return m1;
}

Header parseHeader(string &header)
{
    Header h;

    h.lastRev=get_string_between(header,"head\t",";\n");            //Extract the head part from the content
    h.tags=get_string_between(header,"symbols",";\n");              //Extract tags from the pattern
    return h;
}

map<string,Log> parseLog(string &log)
{

    string temp,prev_content,temp_content;                          //Temporary variables: prev_content holds content of previous revision
    vector<string> SplitLog,temp_log,temp_string;                   //SplitLog will have contents of each revision
    int i=0,j=0,k=0;
    split_string(SplitLog,log,"\n\n\n","");
    Log l,prev;
    int no_of_lines=0,position=0;
    map<string, Log> m1;                                            //Holds the log data for each revision

    for(i=0;i<SplitLog.size();++i)
    {
        temp=SplitLog.at(i);                                        //Splits each revision
        if(temp[0]=='\n')
            temp=temp.substr(1,temp.size());
        l.rev=get_string_between(temp,"","\n");                     //Extract the revision part
        l.msg=get_string_between(temp,"log\n@","\n@\ntext");        //Extract the commit message
        temp_content=get_string_between(temp,"@\ntext\n@","@\n");   //Extract the content and save it on a temporary variable, we will have to re-make content based on previous version content
        temp_log.clear();                                           // clear temporary vector
        temp_string.clear();
        if(!prev_content.empty())
        {
            split_string(temp_log,temp_content,"\n","");            // Split the content line by line
            split_string(temp_string,prev_content,"\n","");         // Split the previous content line by line

            for(j=0;j<temp_log.size();++j)
            {
                if(temp_log.at(j)[0]=='a')
                {
                    position=atoi(get_string_between(temp_log.at(j),"a"," ").c_str())-1;          //Get position to add line
                    no_of_lines=atoi(get_string_between(temp_log.at(j)," ","\n").c_str());      //Get no of lines to be added
                    if(no_of_lines>0)
                    {
                        for(k=1;k<=no_of_lines;++k)
                        {
                            add_line(temp_string,temp_log.at(j+k),position);                    //Add line
                        }
                    }
                }
                else if(temp_log.at(j)[0]=='d')
                {
                    position=atoi(get_string_between(temp_log.at(j),"d"," ").c_str())-1;
                    no_of_lines=atoi(get_string_between(temp_log.at(j)," ","\n").c_str());
                    if(no_of_lines>0)
                        remove_line(temp_string,position,no_of_lines);                          //Remove line
                }
            }
            l.content="";

            for(k=0;k<temp_string.size();k++)
            {
                l.content=l.content.append(temp_string.at(k));                                 //Copy the final content to object l
            }
        }
        else
        {
            l.content=temp_content;
        }
        m1.insert(pair<string,Log>(l.rev,l));                                           //Insert into map
        prev_content=l.content;                                                         //Set as previous content
    }
    return m1;
}

string get_file(string &file)
{
    string content;
    ifstream infile;
    string temp;
    infile.open(file.c_str());
    if(!infile)
    {
        return "";
    }
    while(!infile.eof())
    {
        getline(infile,temp);
        content.append(temp+"\n");
    }
    infile.close();
    return content;
}

void split_string(vector<string> &result, const string &main, const string &search, const string &sub)
{
    if(!main.empty())
    {
        int i=0,pos=0,start=0,len=0;
        vector<int> pos_list_2;

        pos_list.clear();
        pos_list_2=find_pos(main,search,sub,0);

        if(pos_list_2.size()>0)
        {
            vector<string>::iterator sPos;

            for(i=0;i<pos_list_2.size();i++)
            {
                pos=pos_list_2.at(i);
                if(pos<0)
                    break;
                sPos=result.end();
                len=pos-start;
                result.insert(sPos,main.substr(start,len));
                start=pos;
            }

            sPos=result.end();
            len=main.size()-start;
            result.insert(sPos,main.substr(start,len-1));
        }
    }
}

vector<int> find_pos(const string &main,const string &search, const string &sub, int pos)
{
    int sub_pos;
    int i=1;

    i++;

    if(!main.empty() && !search.empty() && pos<main.size() && pos>=0 )
    {
        pos=main.find(search,pos);
        if(pos>=0)
        {
            if(!sub.empty())
            {
                sub_pos=main.find(sub,pos);
                if(sub_pos<pos && sub_pos>main.size())
                    return pos_list;
                else
                    pos=sub_pos+sub.size();
            }
            else
            {
                pos+=search.size();
            }

            it=pos_list.end();
            pos_list.insert(it,pos);
            if(pos<main.size())
                pos_list=find_pos(main,search,sub,pos);
        }
    }
    return pos_list;
}

string get_string_between( const string &main, const string &start, const string &end, int pos)
{
    string result="\0";
    int s_start=0,s_end=0,size=0;
    size=main.size();
    if(!start.empty())
        s_start=main.find(start,pos);
    if(s_start>=0)
    {
        s_start+=start.size();
        if(!end.empty())
        {
            s_end=main.find(end,s_start);
        }
        else
            s_end=main.size();
        if(s_end>=0)
        {
            s_end=s_end-s_start;
            result=main.substr(s_start,s_end);
        }
    }
    return result;
}

void remove_line(vector<string> &main, int &pos, int &no_of_lines)
{
    for(int i=0;i<no_of_lines;++i)
    {
        if(pos<=main.size())
        {
            vector<string>::iterator str=main.begin()+pos;
            main.erase(main.begin()+pos);
            pos+=1;
        }
    }
}

void add_line(vector<string> &main, string &value, int &pos)
{
    main.insert(main.begin()+pos,value);
}
