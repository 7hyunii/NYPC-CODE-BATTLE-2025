#include <iostream>
#include <array>
#include <vector>
#include <algorithm>

using namespace std;

enum DiceRule {
    UNKNOWN = 0,
    ONE, TWO, THREE, FOUR, FIVE, SIX,
    CHOICE, FOUR_OF_A_KIND, FULL_HOUSE, SMALL_STRAIGHT, LARGE_STRAIGHT, YACHT
};

DiceRule stor(const string& s) {
    if (s == "ONE") return ONE;
    if (s == "TWO") return TWO;
    if (s == "THREE") return THREE;
    if (s == "FOUR") return FOUR;
    if (s == "FIVE") return FIVE;
    if (s == "SIX") return SIX;
    if (s == "CHOICE") return CHOICE;
    if (s == "FOUR_OF_A_KIND") return FOUR_OF_A_KIND;
    if (s == "FULL_HOUSE") return FULL_HOUSE;
    if (s == "SMALL_STRAIGHT") return SMALL_STRAIGHT;
    if (s == "LARGE_STRAIGHT") return LARGE_STRAIGHT;
    if (s == "YACHT") return YACHT;
    // == dubug ==
    cerr << "Invalid rule: string to rule not found" << endl;
    // ===========
    return UNKNOWN;
}

string rtos(DiceRule rule) {
    switch (rule) {
        case ONE: return "ONE";
        case TWO: return "TWO";
        case THREE: return "THREE";
        case FOUR: return "FOUR";
        case FIVE: return "FIVE";
        case SIX: return "SIX";
        case CHOICE: return "CHOICE";
        case FOUR_OF_A_KIND: return "FOUR_OF_A_KIND";
        case FULL_HOUSE: return "FULL_HOUSE";
        case SMALL_STRAIGHT: return "SMALL_STRAIGHT";
        case LARGE_STRAIGHT: return "LARGE_STRAIGHT";
        case YACHT: return "YACHT";
        default:{
            // == dubug ==
            cerr << "Invalid rule: rule to string not found" << endl;
            // ===========
            return "UNKNOWN";
        }
    }
}

int evaluate(DiceRule rule, const vector<int>& dice){
    int cnt[7] = {};
    int sum = 0;
    for(int dnum : dice){
        cnt[dnum]++;
        sum += dnum;
    }
    switch(rule){
        case ONE:       return cnt[1] * 1 * 1000;
        case TWO:       return cnt[2] * 2 * 1000;
        case THREE:     return cnt[3] * 3 * 1000;
        case FOUR:      return cnt[4] * 4 * 1000;
        case FIVE:      return cnt[5] * 5 * 1000;
        case SIX:       return cnt[6] * 6 * 1000;
        case CHOICE:    return sum * 1000;
        case FOUR_OF_A_KIND: {
            for(int i = 1; i <= 6; i++)
                if(cnt[i] >= 4) return sum * 1000;
            return 0;
        }
        case FULL_HOUSE: {
            bool pair = false, triple = false;
            for(int i = 1; i <= 6; i++){
                if(cnt[i] == 2 || cnt[i] == 5) pair = true;
                if(cnt[i] == 3 || cnt[i] == 5) triple = true;
            }
            return (pair && triple) ? sum * 1000 : 0;
        }
        case SMALL_STRAIGHT: {
            if((cnt[1] && cnt[2] && cnt[3] && cnt[4]) ||
               (cnt[2] && cnt[3] && cnt[4] && cnt[5]) ||
               (cnt[3] && cnt[4] && cnt[5] && cnt[6])) return 15000;
            return 0;
        }
        case LARGE_STRAIGHT: {
            if((cnt[1] && cnt[2] && cnt[3] && cnt[4] && cnt[5]) ||
               (cnt[2] && cnt[3] && cnt[4] && cnt[5] && cnt[6])) return 30000;
            return 0;
        }
        case YACHT:{
            for(int i = 1; i <= 6; i++)
                if(cnt[i] == 5) return 50000;
            return 0;
        }
        default: {
            // == dubug ==
            cerr << "Invalid rule: rule not found" << endl;
            // ===========
            return 0;
        }
    }
}

struct GameState {
    vector<int> curDice;    // current dice
    vector<int> diceA;      // possible deck if one gets dice group A
    vector<int> diceB;      // possible deck if one gets dice group B
    int totalScore;
    int upperScore;
    array<bool, 13> isAvailRule;

    GameState() : isAvailRule({false}), totalScore(0), upperScore(0) {
        for(int i = 1; i <= 12; i++) isAvailRule[i] = true;
    }

    void getScore(DiceRule rule, const vector<int>& dice){
        // == dubug ==
        if(!isAvailRule[rule]){ cerr << "Invalid rule: rule already used" << endl; return; }
        // ===========
        int amount = evaluate(rule, dice);
        totalScore += amount;
        if(ONE <= rule && rule <= SIX){
            if(upperScore < 63000 && (upperScore + amount) >= 63000) totalScore += 35000;
            upperScore += amount;
        }
        isAvailRule[rule] = false;
        for(int d : dice){
            auto itr = find(curDice.begin(), curDice.end(), d);
            if(itr != curDice.end()) curDice.erase(itr);
            // == dubug ==
            else cerr << "Invalid dice: dice " << d << " not found" << endl;
            // ===========
        }
    }
};

struct Bid {
    char group;
    int amount;
};

class PlayerAI {
    GameState me;
    GameState op;
    Bid mybid;

    void ready(){
        cout << "OK" << endl;
    }
    
    void roll(const string& sa, const string& sb){
        me.diceA = me.curDice;
        op.diceA = op.curDice;
        for(const char& c : sa){
            int dnum = c - '0';
            me.diceA.push_back(dnum);
            op.diceA.push_back(dnum);
        }
        me.diceB = me.curDice;
        op.diceB = op.curDice;
        for(const char& c : sb){
            int dnum = c - '0';
            me.diceB.push_back(dnum);
            op.diceB.push_back(dnum);
        }

        char g;
        int x;
        // ==============================
        // bidding algorithm here
        g = 'A';
        x = 100;
        // ==============================
        mybid.group = g;
        mybid.amount = x;
        cout << "BID " << g << " " << x << endl;
    }
    
    void get(char g, char g0, int x0){
        if(g == 'A'){
            me.curDice = me.diceA;
            op.curDice = op.diceB;
        }
        else{
            me.curDice = me.diceB;
            op.curDice = op.diceA;
        }

        if(mybid.group == g)    me.totalScore -= mybid.amount;
        else                    me.totalScore += mybid.amount;

        if(g != g0) op.totalScore -= x0;
        else        op.totalScore += x0;
    }
    
    void score(){
        DiceRule rule;
        vector<int> selected;
        // ==============================
        // scoring algorithm here
        for(int i = 1; i <= 12; i++){
            if(me.isAvailRule[i]){
                rule = static_cast<DiceRule>(i);
                break;
            }
        }
        
        selected = vector<int>(me.curDice.begin(), me.curDice.begin() + 5);
        // ==============================
        me.getScore(rule, selected);
        cout << "PUT " << rtos(rule) << " ";
        for(int d : selected) cout << d;
        cout << endl;
    }
    
    void set(const string& rc, const string& sd){
        DiceRule rule = stor(rc);
        vector<int> opdice;
        for(const char& c : sd)
            opdice.push_back(c - '0');
        op.getScore(rule, opdice);
    }

public:
    void play(){
        string cmd;

        while(true){
            cin >> cmd;

            if(cmd == "READY"){
                ready();
            }
            else if(cmd == "ROLL"){
                string sa, sb;
                cin >> sa >> sb;
                roll(sa, sb);
            }
            else if(cmd == "GET"){
                char g, g0;
                int x0;
                cin >> g >> g0 >> x0;
                get(g, g0, x0);
            }
            else if(cmd == "SCORE"){
                score();
            }
            else if(cmd == "SET"){
                string c, sd;
                cin >> c >> sd;
                set(c, sd);
            }
            else if(cmd == "FINISH"){
                break;
            }
        }
    }
};

int main(){
    PlayerAI ai;
    ai.play();
    return 0;
}