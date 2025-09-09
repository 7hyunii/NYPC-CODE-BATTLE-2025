#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <chrono>
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
    cerr << "Invalid rule: string to rule not found" << endl;
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
            cerr << "Invalid rule: rule to string not found" << endl;
            return "UNKNOWN";
        }
    }
}

// evaluates the score of 5 dices
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
            // 요트(5 of a kind)도 풀하우스 포함
            bool pair = false, triple = false;
            for(int i = 1; i <= 6; i++){
                if(cnt[i] == 5) { pair = true; triple = true; } // 포함
                if(cnt[i] == 2) pair = true;
                if(cnt[i] == 3) triple = true;
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
            cerr << "Invalid rule: rule not found" << endl;
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
    int remainingRules;

    GameState() : isAvailRule({false}), totalScore(0), upperScore(0), remainingRules(12) {
        for(int r = 1; r <= 12; r++) isAvailRule[r] = true;
    }

    void getScore(DiceRule rule, const vector<int>& dice){
        if(!isAvailRule[rule]){ cerr << "Invalid rule: rule already used" << endl; return; }
        int amount = evaluate(rule, dice);
        totalScore += amount;
        if(ONE <= rule && rule <= SIX){
            if(upperScore < 63000 && (upperScore + amount) >= 63000) totalScore += 35000;
            upperScore += amount;
        }
        isAvailRule[rule] = false;
        remainingRules--;
        for(int d : dice){
            auto itr = find(curDice.begin(), curDice.end(), d);
            if(itr != curDice.end()) curDice.erase(itr);
            else cerr << "Invalid dice: dice " << d << " not found" << endl;
        }
    }
};

class PlayerAI {

    struct Bid { char group; int amount; };
    struct Potentials {
        int combScore = -1;
        int combStratVal = -1;
        int potentialVal = -1;
        int totalVal = -9999999;
    };
    struct DiceComb {
        DiceRule rule;
        vector<int> dice;
        Potentials ptts;
    };

    GameState me;
    GameState op;
    Bid mybid;

    double dTarg = 0.65;       // dampening target
    double dampeningFactor = 1.0;

    Bid opPredBid;
    const double LEARNING_RATE = 0.15;
    double opAggressFactor = 1.0;

    // 누적 과지출(내가 낸 입찰 - 내가 받은 입찰)
    int netBidBalance = 0;

    // [LATE] 라운드 후반 가중치(남은 룰 수 기준: 12 -> 초반, 0 -> 종료 직전)
    double lateWeight(int remainingRules) const {
        if (remainingRules <= 2) return 1.00; // 마지막 2라운드: 극공
        if (remainingRules <= 4) return 0.80;
        if (remainingRules <= 6) return 0.55;
        if (remainingRules <= 8) return 0.35;
        return 0.20; // 초반
    }

    int calcStratBonus(DiceRule rule, int combScore){
        if(combScore == 0) return 0;
        switch (rule) {
            case ONE: {     int dcnt = combScore / 1000;
                if(dcnt == 1)   return 2500;
                if(dcnt == 2)   return 2500;
                if(dcnt == 3)   return 2500;
                if(dcnt == 4)   return 4000;
                return 4000;
            }
            case TWO: {     int dcnt = combScore / 2000;
                if(dcnt == 1)   return 1000;
                if(dcnt == 2)   return 1000;
                if(dcnt == 3)   return 1000;
                if(dcnt == 4)   return 4000;
                return 4000;
            }
            case THREE: {   int dcnt = combScore / 3000;
                if(dcnt == 1)   return -3000;
                if(dcnt == 2)   return -3000;
                if(dcnt == 3)   return -1000;
                if(dcnt == 4)   return 3000;
                return 3000;
            }
            case FOUR: {    int dcnt = combScore / 4000;
                if(dcnt == 1)   return -6000;
                if(dcnt == 2)   return -6000;
                if(dcnt == 3)   return 3000;
                if(dcnt == 4)   return 1000;
                return 3000;
            }
            case FIVE: {    int dcnt = combScore / 5000;
                if(dcnt == 1)   return -8000;
                if(dcnt == 2)   return -8500;
                if(dcnt == 3)   return -2000;
                if(dcnt == 4)   return 3000;
                return 5000;
            }
            case SIX: {     int dcnt = combScore / 6000;
                if(dcnt == 1)   return -10000;
                if(dcnt == 2)   return -11000;
                if(dcnt == 3)   return -7000;
                if(dcnt == 4)   return 3000;
                return 5000;
            }
            case CHOICE: return -16500;
            case FOUR_OF_A_KIND:    return 3500;
            case FULL_HOUSE:        return 2500;
            case SMALL_STRAIGHT:    return 3000;
            case LARGE_STRAIGHT:    return 0;
            case YACHT:             return 0;
            default: return 0;
        }
    }

    int calcCombPotential(DiceRule rule, const vector<int>& dice){
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
            case CHOICE:    return sum * 700;
            case FOUR_OF_A_KIND: {
                for(int i = 1; i <= 6; i++){
                    if(cnt[i] != 3) continue;
                    for(int j = 6; j >= 1; j--){
                        if(cnt[j] == 1 || cnt[j] == 2) return (i * 4 + j) * 1000;
                    }
                    break;
                }
                return 0;
            }
            case FULL_HOUSE: {
                int tripleSum = 0;
                int pairCnt = 0, pairSum = 0;
                for(int i = 1; i <= 6; i++){
                    if(cnt[i] == 2) { pairCnt++; pairSum += i * 2; }
                    if(cnt[i] == 3) tripleSum = i * 3;
                    if(cnt[i] == 4) return (i * 5) * 1000;
                }
                if(pairCnt == 2){
                    for(int j = 6; j >= 1; j--){
                        if(cnt[j] == 2) return (pairSum + j) * 1000;
                    }
                }
                if(tripleSum){
                    for(int j = 6; j >= 1; j--){
                        if(cnt[j] == 1 || cnt[j] == 2) return (tripleSum + j * 2) * 1000;
                    }
                }
                return 0;
            }
            case SMALL_STRAIGHT: {
                for(int add = 1; add <= 6; add++){
                    cnt[add]++;
                    if((cnt[1] && cnt[2] && cnt[3] && cnt[4]) ||
                       (cnt[2] && cnt[3] && cnt[4] && cnt[5]) ||
                       (cnt[3] && cnt[4] && cnt[5] && cnt[6])) return 11250;
                    cnt[add]--;
                }
                return 0;
            }
            case LARGE_STRAIGHT: {
                const int pattern[2][5] = { {1, 2, 3, 4, 5}, {2, 3, 4, 5, 6} };
                int minReq = 3;
                for(int i = 0; i < 2; i++){
                    int diceReq = 0;
                    for(int j = 0; j < 5; j++){
                        if(!cnt[pattern[i][j]]) diceReq++;
                    }
                    if(diceReq < minReq) minReq = diceReq;
                }
                if(minReq == 2) return 18000;
                if(minReq == 1) return 24000;
                return 0;
            }
            case YACHT:{
                for(int i = 1; i <= 6; i++){
                    if(cnt[i] == 3) return 20000;
                    if(cnt[i] == 4) return 30000;
                }
                return 0;
            }
            default: return 0;
        }
    }

    // calculates the potential score of given dice combination
    Potentials calcPotential(DiceRule rule, const vector<int>& selected, const vector<int>& notSelected, const GameState& state){
        int combScore = evaluate(rule, selected);
        int combStrat = calcStratBonus(rule, combScore);

        bool gotUpperBonus = false;
        int futureUpperBase = state.upperScore;
        if(ONE <= rule && rule <= SIX){
            futureUpperBase += combScore;
            if(state.upperScore < 63000 && (state.upperScore + combScore) >= 63000){ combScore += 35000; gotUpperBonus = true; }
        }

        if(notSelected.size() != 5) return {combScore, combStrat, 0, combScore + combStrat};

        int potential = 0;
        for(int r = 12; r >= 1; r--){
            if(!state.isAvailRule[r] || r == rule) continue;

            int definitePtt = evaluate(static_cast<DiceRule>(r), notSelected);
            if(!gotUpperBonus && ONE <= r && r <= SIX){
                if(futureUpperBase < 63000 && (futureUpperBase + definitePtt) >= 63000) definitePtt += 35000;
            }

            int combPtt = 0;
            if(state.remainingRules > 2) combPtt = calcCombPotential(static_cast<DiceRule>(r), notSelected);
            int dpCombPtt = (int)(double(combPtt) * dampeningFactor);
            int resultPtt = (definitePtt > dpCombPtt) ? definitePtt : dpCombPtt;

            int aliveDiceBonus = 0;
            for(int d : notSelected){
                if(!state.isAvailRule[d] || d == rule) continue;
                aliveDiceBonus += (d * 5);
            }
            resultPtt += aliveDiceBonus;

            if(resultPtt > potential) potential = resultPtt;
        }
        return {combScore, combStrat, potential, combScore + combStrat + potential};
    }

    DiceComb findBestComb(const vector<int>& dice, const GameState& state){
        DiceComb best;
        vector<bool> v(dice.size(), false);
        fill(v.begin(), v.begin() + 5, true);
        do{
            vector<int> selected;
            vector<int> notSelected;
            for(int i = 0; i < (int)dice.size(); i++){
                if(v[i])    selected.push_back(dice[i]);
                else        notSelected.push_back(dice[i]);
            }

            for(int r = 12; r >= 1; r--){
                if(!state.isAvailRule[r]) continue;
                DiceRule rule = static_cast<DiceRule>(r);
                Potentials ptts = calcPotential(rule, selected, notSelected, state);
                
                if(( ptts.totalVal > best.ptts.totalVal ) ||
                   ( (ptts.totalVal == best.ptts.totalVal) && (ptts.combScore + ptts.combStratVal > best.ptts.combScore + best.ptts.combStratVal) )){
                    best.rule = rule;
                    best.dice = selected;
                    best.ptts = ptts;
                }
            }
        }while(prev_permutation(v.begin(), v.end()));
        return best;
    }
    
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
        // bidding algorithm here  (+ late aggression)  // [LATE]
        double late = lateWeight(me.remainingRules);                // [LATE]
        int minFloor = (me.remainingRules <= 4 ? 250 : 50);         // [LATE]

        int myValA = findBestComb(me.diceA, me).ptts.totalVal;
        int myValB = findBestComb(me.diceB, me).ptts.totalVal;
        int opValA = findBestComb(op.diceA, op).ptts.totalVal;
        int opValB = findBestComb(op.diceB, op).ptts.totalVal;

        char myPrefGroup = (myValA >= myValB) ? 'A' : 'B';
        char opPrefGroup = (opValA >= opValB) ? 'A' : 'B';
        
        int myGain = abs(myValA - myValB);
        int opGain = abs(opValA - opValB);

        int myAffordable = min(myGain / 2, 10000);
        int opAffordable = min(opGain / 2, 10000);

        opPredBid.group = opPrefGroup;
        opPredBid.amount = opAffordable;

        g = myPrefGroup;

        if(myPrefGroup != opPrefGroup){
            x = 3;
        } else {
            if(myGain > opGain){
                int baseBid = (int)((double)opAffordable * 0.9 * opAggressFactor);
                int premium = (int)((double)myAffordable * 0.2);
                x = baseBid + premium;
            } else {
                x = min((int)((float)opAffordable * 0.85 * opAggressFactor), myAffordable);
            }

            // 추가 가중(pressure) 계산
            double PRESS_ADV  = 0.40;  // max(0, myGain - opGain)의 계수
            double PRESS_CONF = 0.25;  // myGain의 계수
            double PRESS_CAP_RATIO = 0.40;  // pressure 상한 (avgAffordable의 %)

            int pressure = (int)( late * ( PRESS_ADV * max(0, myGain - opGain)
                                         + PRESS_CONF * myGain ) );

            // pressure 캡
            int avgAffordable = (myAffordable + opAffordable) / 2;
            int pressureCap   = (int)(PRESS_CAP_RATIO * avgAffordable);
            if (pressure > pressureCap) pressure = pressureCap;

            // 우선 바닥을 확보한 뒤
            int x0 = x + pressure;
            x0 = max(minFloor, x0);

            // ====== (A) WTP 상한: 가치차보다 비싸게 사지 말기 ======
            int valueAdv = max(0, myGain - opGain);                 // 순가치
            double wtpFrac = (me.remainingRules <= 4) ? 0.95 : 0.70; // 후반↑
            int wtpCap = (int)(wtpFrac * valueAdv);
            if (wtpCap >= 0) x0 = min(x0, wtpCap);

            // ====== (B) ROI 가드: (이득-가격)/가격 >= ROI_MIN ======
            double ROI_MIN = 0.10;                                   // 최소 10%
            if (x0 > 0) {
                int netGain = valueAdv - x0;
                if (netGain < (int)(ROI_MIN * x0)) {
                    int roiCap = (int)( valueAdv / (1.0 + ROI_MIN) ); // x <= valueAdv/(1+ROI)
                    if (roiCap >= 0) x0 = min(x0, roiCap);
                }
            }

            // ====== (C) 리드/추격 스로틀 ======
            int lead = me.totalScore - op.totalScore;
            const int LEAD_SOFT = 15000;
            if (lead > LEAD_SOFT) {
                x0 = (int)(x0 * 0.85);
            } else if (lead < -LEAD_SOFT) {
                x0 = (int)(x0 * 1.08);
            }

            // ====== (D) 예산 스로틀(누적 과지출 시 감산) ======
            const int SPEND_BUDGET = 12000;
            if (netBidBalance > SPEND_BUDGET) {
                x0 = (int)(x0 * 0.75);
            }

            // 최종 클램프 (과소비 방지 후)
            x = min(9999, max(0, x0));
        }
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

        if(mybid.group == g) {
            me.totalScore -= mybid.amount;
            netBidBalance += mybid.amount;   // ▲ 내가 낸 금액 누적
        } else {
            me.totalScore += mybid.amount;
            netBidBalance -= mybid.amount;   // ▼ 내가 받은 금액 누적
        }

        if(g != g0) op.totalScore -= x0;
        else        op.totalScore += x0;

        if(g0 == opPredBid.group && x0 >= 50 && opPredBid.amount > 50){
            double ratio = (double)x0 / (double)opPredBid.amount;
            opAggressFactor = opAggressFactor * (1.0 - LEARNING_RATE) + ratio * LEARNING_RATE;
        }
    }
    
    void score(){
        DiceComb selection = findBestComb(me.curDice, me);
        me.getScore(selection.rule, selection.dice);
        double t = (me.remainingRules - 1) / 11.0;
        dampeningFactor = dTarg + (1.0 - dTarg) * t;
        cout << "PUT " << rtos(selection.rule) << " ";
        for(int d : selection.dice) cout << d;
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
