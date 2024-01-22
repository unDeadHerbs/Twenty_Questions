#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
namespace fs = std::filesystem;

// Helper for variant::visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

template<typename T>
bool in(T e,std::vector<T> const v){ return std::ranges::find(v, e) != v.end(); }

using std::literals::string_literals::operator""s;
std::string operator+(int const lhs, std::string const& rhs){ return std::to_string(lhs) + rhs; }
std::string operator+(std::string const& lhs, int const rhs){ return lhs + std::to_string(rhs); }

typedef std::string Answer;
struct Question{
	Question() = default;
	std::string property;
	int yes;
	int no;
	Question(int y,int n,std::string p):yes(y),no(n),property(p){}
	std::string serialize() const{ return yes + ","s + no  + ","s + property; }
	Question(std::string serial){
		// There's probably a better way to extract the pattern.
		std::istringstream iss(serial);
		std::string token;
		std::getline(iss,token,',');
		std::istringstream(token) >> yes;
		std::getline(iss,token,',');
		std::istringstream(token) >> no;
		std::getline(iss,property,',');
	}
};

typedef std::variant<Question,Answer> QA;

#define NEWGAME -1
#define GAMEOVER -2

struct QAdb{
	std::vector<QA> qas;
	int start;
	bool changed = false;
	QAdb():qas({(Answer)"Cat"}),start(0){}
	auto& operator[](int p){return qas[p];}
	int add_answer(std::string ans){
		// TODO: Deduplicate
		qas.push_back((Answer)ans);
		changed = true;
		return qas.size()-1;
	}
	int add_question(int parent,bool yorn,std::string prop,int yes,int no){
		// TODO: Store the property string as an `Answer` and use the index here.
		qas.push_back((Question){yes,no,prop});
		changed = true;
		return (NEWGAME==parent
						? start 
						: std::get<Question>(qas[parent]).*(yorn
																								? &Question::yes
																								: &Question::no))
			= qas.size()-1;
	}
	std::string serialize() const{
		return
			"s,"+std::to_string(start)+"\n"+
			std::transform_reduce(qas.begin(), qas.end(),
														std::string(""), std::plus{},
														[](QA qa){return std::visit(overloaded{
																	[](Question q){return "q,"+q.serialize();},
																	[](Answer a){return "a,"+(std::string)a;}},qa)+"\n";});		
	}
	void deserialize(std::string serial){
		qas.clear();
		// TODO: Find a better way with std::ranges and a pattern matcher.
		std::istringstream iss(serial);
		std::string line;
		while (std::getline(iss, line)) {
			if(line.starts_with("a,"))
				qas.push_back((Answer)line.substr(2));
			else if(line.starts_with("q,"))
				qas.push_back((Question)line.substr(2));
			else if(line.starts_with("s,"))
				start = std::stoi(line.substr(2));
			else
				std::cerr<<"[ERROR]: Invalid line: "<<line<<std::endl;
		}
	}
} qadb;

bool isYes(const std::string& answer){ return in(answer, {"yes","y","yep"}); }
bool isNo(const std::string& answer){ return in(answer, {"no","n","nope"}); }
bool isInvalid(const std::string& answer){ return in(answer, {"invalid","i"}); }

std::string ask_str(const std::string& question){
	std::cout << question << "\n : ";
	std::string answer;
	std::getline(std::cin,answer);
	return answer;
}
bool ask_yn(const std::string& question) {
	while(true){
		std::string answer = ask_str(question+"\n(Enter 'yes' or 'no')");
		std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
		if(isYes(answer)) return true;
		if(isNo(answer)) return false;
	}
}
std::optional<bool> ask_yni(const std::string& question) {
	while(true){
		std::string answer = ask_str(question + "\n(Enter 'yes', 'no', or 'invalid')");
		std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
		if(isYes(answer)) return true;
		if(isNo(answer)) return false;
		if(isInvalid(answer)) return {};
	}
}

bool last_answer = false;
int last_pos = NEWGAME;
void add_question(int pos, std::string lprompt, std::string rpropmpt){
	std::string answer = ask_str("What were you thinking of?");
	// TODO: If already an answer, find departing question and check validity?
	int ans_id = qadb.add_answer(answer);
	std::string property = ask_str(lprompt+answer+rpropmpt);
	if(ask_yn("Does a \""+answer+"\" have \""+property+"\"?"))
		qadb.add_question(last_pos,last_answer,property,ans_id,pos);
	else
		qadb.add_question(last_pos,last_answer,property,pos,ans_id);
}
int ask_play(int pos, Question const q){
	std::string question = "Does it have \""+q.property+"\"?";
	auto ans = ask_yni(question);
	if(ans) return (last_answer = *ans)? q.yes : q.no;
	// else the question was invalid
	add_question(pos,"What property or attribute does \"","\" have or not have that makes the question \""+question+"\" invalid?");
	return GAMEOVER;
}
int ask_play(int pos, Answer const a){
	std::cout << "My guess is \""<<a<<"\".\n";
	if(!ask_yn("Was I correct?"))
		add_question(pos,"What is a property or attribute that would differentiate \"","\" from \""+a+"\"?");
	return GAMEOVER;
}
void play(int pos=NEWGAME){
	if(NEWGAME==pos){ last_pos=NEWGAME; pos = qadb.start; }
	if(GAMEOVER==pos) return;
	int next = std::visit([pos](auto const qa){ return ask_play(pos,qa);},qadb[pos]);
	last_pos=pos;
	play(next);
}

int main(){
	fs::path db_path = "database.csv";
	if(fs::exists(db_path)){
		std::ifstream db_file(db_path);
		if(!db_file) return EXIT_FAILURE;
		std::string db_text({std::istreambuf_iterator<char>(db_file),
		                     std::istreambuf_iterator<char>()});
		qadb.deserialize(db_text);
	}

	do play();
	while(ask_yn("Play again?"));

	if(qadb.changed){
		std::ofstream db_file(db_path);
		if(!db_file) return EXIT_FAILURE;
		db_file << qadb.serialize();
	}
	return 0;
}
