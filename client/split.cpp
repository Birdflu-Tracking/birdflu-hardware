#include <iostream>
#include <vector>

using namespace std;

int main() {
	string str = "write ONQWS4TBNIQGWYLQMRUQU===";
	int len = str.length(), pos = 0;
	vector<string> strs;	

	while(1) {
		if (str[pos] == ' ') {
			strs.push_back(str.substr(0, pos));
			break;
		}
		pos++;
	}

	strs.push_back(str.substr(pos+1));
	for (int i = 0; i < strs.size(); i++) {
		cout<<strs[i];
	}
	return 0;
}
