#include <bits/stdc++.h>
using namespace std;

/*** Yacht Auction Bot — Stable-Conservative Bidding (C++17) ***/

enum Cat {
    ONE, TWO, THREE, FOUR, FIVE, SIX,
    CHOICE, FOUR_OF_A_KIND, FULL_HOUSE, SMALL_STRAIGHT, LARGE_STRAIGHT, YACHT,
    CAT_N
};
static const vector<string> CAT_NAME = {
    "ONE","TWO","THREE","FOUR","FIVE","SIX",
    "CHOICE","FOUR_OF_A_KIND","FULL_HOUSE","SMALL_STRAIGHT","LARGE_STRAIGHT","YACHT"
};

struct State {
    array<bool, CAT_N> used{};
    long long upper_sum = 0;   // 상단합(×1000)
    long long bid_acc   = 0;   // 입찰 가/감 누적
    long long my_pts    = 0;   // PUT 점수(보너스 제외)
    long long opp_pts   = 0;   // 상대 PUT 점수(추정)

    vector<int> older5;        // 이전 라운드 carry-over 5개
    vector<int> newer5;        // 이번 GET 5개

    vector<int> lastA, lastB;  // 직전 ROLL
    char desire_group = 'A';
    int  bid_value    = 0;

    int get_count   = 0;       // GET 횟수(≈라운드 번호)
    int score_count = 0;       // SCORE 횟수
};

// ---------- 유틸 ----------
static inline vector<int> parse5(const string& s){
    vector<int> v; v.reserve(5);
    for(char ch: s){
        if('0'<=ch && ch<='9'){
            v.push_back(ch - '0');
            if((int)v.size()==5) break;
        }
    }
    while((int)v.size()<5) v.push_back(1);
    return v;
}
static inline bool is_upper(Cat c){ return c>=ONE && c<=SIX; }

static long long score_category(const array<int,5>& d, Cat c){
    array<int,7> cnt{}; for(int x: d) cnt[x]++;
    int s=0;
    switch(c){
        case ONE:   for(int x: d) if(x==1) s+=1;  return 1000LL*s;
        case TWO:   for(int x: d) if(x==2) s+=2;  return 1000LL*s;
        case THREE: for(int x: d) if(x==3) s+=3;  return 1000LL*s;
        case FOUR:  for(int x: d) if(x==4) s+=4;  return 1000LL*s;
        case FIVE:  for(int x: d) if(x==5) s+=5;  return 1000LL*s;
        case SIX:   for(int x: d) if(x==6) s+=6;  return 1000LL*s;
        case CHOICE: return 1000LL*(d[0]+d[1]+d[2]+d[3]+d[4]);
        case FOUR_OF_A_KIND: {
            for(int v=1; v<=6; ++v) if(cnt[v]>=4) return 1000LL*(d[0]+d[1]+d[2]+d[3]+d[4]);
            return 0;
        }
        case FULL_HOUSE: { // 5-kind도 인정
            bool five=false, h3=false, h2=false;
            for(int v=1; v<=6; ++v){
                if(cnt[v]==5) five=true;
                if(cnt[v]==3) h3=true;
                if(cnt[v]==2) h2=true;
            }
            return (five || (h3 && h2)) ? 1000LL*(d[0]+d[1]+d[2]+d[3]+d[4]) : 0;
        }
        case SMALL_STRAIGHT: {
            array<bool,7> p{}; for(int x: d) p[x]=true;
            bool ok = (p[1]&&p[2]&&p[3]&&p[4]) || (p[2]&&p[3]&&p[4]&&p[5]) || (p[3]&&p[4]&&p[5]&&p[6]);
            return ok ? 15000LL : 0;
        }
        case LARGE_STRAIGHT: {
            array<bool,7> p{}; for(int x: d) p[x]=true;
            bool ok = (p[1]&&p[2]&&p[3]&&p[4]&&p[5]) || (p[2]&&p[3]&&p[4]&&p[5]&&p[6]);
            return ok ? 30000LL : 0;
        }
        case YACHT: {
            for(int v=1; v<=6; ++v) if(cnt[v]==5) return 50000LL;
            return 0;
        }
        default: return 0;
    }
}

static inline void gen_combinations(const vector<int>& pool, vector<array<int,5>>& out){
    int n = (int)pool.size(); if(n<5) return;
    vector<int> pick(5);
    function<void(int,int)> dfs = [&](int start, int k){
        static array<int,5> tmp;
        if(k==5){
            for(int i=0;i<5;++i) tmp[i]=pool[pick[i]];
            sort(tmp.begin(), tmp.end());
            out.push_back(tmp);
            return;
        }
        for(int i=start; i<=n-(5-k); ++i){ pick[k]=i; dfs(i+1,k+1); }
    };
    dfs(0,0);
}

static inline vector<int> leftover_after_use(const vector<int>& pool, const array<int,5>& usedDice){
    array<int,7> cnt{}; for(int x: pool) cnt[x]++; for(int x: usedDice) cnt[x]--;
    vector<int> left; left.reserve(5);
    for(int v=1; v<=6; ++v) while(cnt[v]-- > 0) left.push_back(v);
    return left;
}
static inline bool can_make_from_pool(const vector<int>& pool, const array<int,5>& d){
    array<int,7> cnt{}; for(int x: pool) cnt[x]++; for(int x: d) if(--cnt[x] < 0) return false; return true;
}
static inline string dice_str(const array<int,5>& a5){
    string s; s.reserve(5); for(int i=0;i<5;++i) s.push_back(char('0'+a5[i])); return s;
}

// 희소성 가중(선택/입찰 평가에 소폭 가산)
static inline long long scarcity_bias(int c){
    switch(c){
        case YACHT:           return 5000;
        case LARGE_STRAIGHT:  return 1500;
        case FULL_HOUSE:      return 800;
        case SMALL_STRAIGHT:  return 600;
        case FOUR_OF_A_KIND:  return 400;
        case CHOICE:          return 200;
        default:              return 300; // 상단 소폭
    }
}

// 풀에서 "가중치 포함" 최대값 (입찰 평가)
static inline long long best_value_from_pool_biased(const vector<int>& pool, const array<bool, CAT_N>& used){
    vector<array<int,5>> all5; gen_combinations(pool, all5);
    long long best=0;
    for(const auto& a5: all5){
        for(int c=0;c<CAT_N;++c){
            if(used[c]) continue;
            long long sc = score_category(a5, (Cat)c) + scarcity_bias(c);
            if(is_upper((Cat)c)) sc += 6000; // 상단 보너스 기대치 그림자값
            if(sc > best) best = sc;
        }
    }
    return best;
}

// 묶음 단독 최대값 (상대 선호 추정용)
static inline long long bundle_raw_value(const vector<int>& v5){
    array<int,5> a{v5[0],v5[1],v5[2],v5[3],v5[4]}; sort(a.begin(), a.end());
    long long best=0; for(int c=0;c<CAT_N;++c) best=max(best, score_category(a,(Cat)c)); return best;
}

// 선택(가중 + 다음 라운드 대비 남는 5개 합 타이브레이크)
static inline pair<int, array<int,5>> choose_best_from_pool_biased(
        const vector<int>& pool, const array<bool, CAT_N>& used){
    vector<array<int,5>> all5; gen_combinations(pool, all5);
    long long bestKey = LLONG_MIN;
    int bestCat = CHOICE; array<int,5> bestDice{1,1,1,1,1};
    for(const auto& a5: all5){
        for(int c=0;c<CAT_N;++c){
            if(used[c]) continue;
            long long sc  = score_category(a5, (Cat)c) + scarcity_bias(c);
            vector<int> left = leftover_after_use(pool, a5);
            long long leftSum = accumulate(left.begin(), left.end(), 0) * 10; // 약한 가중
            long long key = sc * 1000 + leftSum;
            if(key > bestKey){ bestKey = key; bestCat = c; bestDice = a5; }
        }
    }
    return {bestCat, bestDice};
}

static inline int parse_cat(const string& s){
    for(int i=0;i<CAT_N;++i) if(CAT_NAME[i]==s) return i;
    return CHOICE;
}

// ---------- 메인 ----------
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    State st;
    string cmd;

    while (cin >> cmd){
        if(cmd=="READY"){
            cout << "OK" << endl;
        }
        else if(cmd=="ROLL"){
            string A,B; cin >> A >> B;
            st.lastA = parse5(A);
            st.lastB = parse5(B);

            // 우리 시너지 가치
            vector<int> poolA = st.older5; poolA.insert(poolA.end(), st.lastA.begin(), st.lastA.end());
            vector<int> poolB = st.older5; poolB.insert(poolB.end(), st.lastB.begin(), st.lastB.end());
            long long vA = best_value_from_pool_biased(poolA, st.used);
            long long vB = best_value_from_pool_biased(poolB, st.used);

            // 상대 선호(묶음 단독)로 충돌 확률 추정
            long long rawA = bundle_raw_value(st.lastA);
            long long rawB = bundle_raw_value(st.lastB);
            char oppPref = (rawA>rawB?'A':(rawB>rawA?'B':'X'));

            char g_me = (vA>=vB ? 'A' : 'B');
            long long delta = llabs(vA - vB);

            // 충돌 확률 q (정적)
            double q_same;
            if(rawA==rawB) q_same = 0.5;
            else{
                bool sameAsOpp = (g_me == oppPref);
                double base = sameAsOpp ? 0.75 : 0.25; // 살짝 더 보수
                long long gap = llabs(rawA - rawB);
                if(gap < 2500) base = 0.55;           // 비슷하면 반반
                if(gap > 9000) base = sameAsOpp ? 0.88 : 0.12;
                q_same = base;
            }

            int roundIdx = st.get_count + 1;          // 이번 GET이 들어올 라운드
            long long margin = st.opp_pts - st.my_pts; // (+) 우리가 뒤짐

            // 라운드 가중 (보수형)
            double alpha = (roundIdx<=4?0.55:(roundIdx<=8?0.85:(roundIdx<=10?1.08:(roundIdx<=12?1.28:1.55))));

            // 앞서면 절약, 뒤지면 약간 공격
            if(margin < -40000) alpha *= 0.80;
            else if(margin > 40000) alpha *= 1.10;
            else if(margin > 80000) alpha *= 1.25;

            // 입찰 임계 (보수형)
            long long tau = 2000;
            if(margin > 60000) tau -= 300;       // 많이 뒤지면 문턱 살짝 낮춤
            if(margin < -40000) tau += 1500;     // 앞서면 문턱 올림

            // q 효과: 낮으면 0, 애매하면 절반
            double q_eff = q_same;
            if(q_eff < 0.50) q_eff = 0.0;
            else if(q_eff < 0.65) q_eff *= 0.5;

            long long x = (long long) floor( q_eff * alpha * max(0LL, delta - tau) );

            // 과금 상한 ①: 이득 대비 과금 제한
            long long capDelta = (long long)(0.85 * delta + 3500);
            if(x > capDelta) x = capDelta;

            // 과금 상한 ②: 라운드별 절대 상한 (막판만 조금 더 허용)
            long long capAbs = (roundIdx<=4? 12000 : (roundIdx<=8? 14000 : (roundIdx<=10? 15000 : 20000)));
            if(x > capAbs) x = capAbs;

            // 앞서면 그냥 0 입찰로 세이브
            if(margin < -30000 && roundIdx <= 11) x = 0;

            // 타이 회피 지터
            int sumA = accumulate(st.lastA.begin(), st.lastA.end(), 0);
            int sumB = accumulate(st.lastB.begin(), st.lastB.end(), 0);
            if(x>0) x += ((sumA + sumB + roundIdx*17) % 3);
            static const int bads[] = {0,150,500,1000,1500,3000,5000,10000};
            for(int b: bads){ if(x==b){ x+=1; break; } }
            if(x>100000) x=100000;
            if(x<0) x=0;

            st.desire_group = g_me;
            st.bid_value    = (int)x;
            cout << "BID " << g_me << " " << x << endl;
        }
        else if(cmd=="GET"){
            char g, g0; int x0; cin >> g >> g0 >> x0;
            st.get_count++;

            // 입찰 점수 반영
            if(g == st.desire_group) st.bid_acc -= st.bid_value;
            else                     st.bid_acc += st.bid_value;

            vector<int> got = (g=='A' ? st.lastA : st.lastB);
            if(st.get_count == 1){ st.older5 = got; st.newer5.clear(); }
            else                  { st.newer5 = got; }
        }
        else if(cmd=="SCORE"){
            st.score_count++;

            // 풀 구성 (R2~R12: 10개, R13: 5개)
            vector<int> pool;
            if(!st.older5.empty()) pool.insert(pool.end(), st.older5.begin(), st.older5.end());
            if(!st.newer5.empty()) pool.insert(pool.end(), st.newer5.begin(), st.newer5.end());
            if((int)pool.size() < 5){
                if(!st.newer5.empty()) pool = st.newer5;
                else                   pool = st.older5;
            }

            // 최적 선택(가중 + leftover 타이브레이크)
            auto choice = choose_best_from_pool_biased(pool, st.used);
            int cat = choice.first;
            array<int,5> d5 = choice.second;

            // ABORT 방지: 멀티셋 검증 + 안전 폴백
            if(!can_make_from_pool(pool, d5)){
                array<int,5> fb{}; for(int i=0;i<5 && i<(int)pool.size(); ++i) fb[i]=pool[i];

                long long best=LLONG_MIN; int bestCat=-1;
                for(int c=0;c<CAT_N;++c){
                    if(st.used[c]) continue;
                    long long sc = score_category(fb,(Cat)c) + (is_upper((Cat)c)?300:0);
                    if(sc>best){ best=sc; bestCat=c; }
                }
                if(bestCat==-1) bestCat=CHOICE;
                cat=bestCat; d5=fb;
            }

            long long gained = score_category(d5,(Cat)cat);
            st.my_pts += gained;
            if(is_upper((Cat)cat)) st.upper_sum += gained;

            st.used[cat]=true;
            cout << "PUT " << CAT_NAME[cat] << " " << dice_str(d5) << endl;

            // 남은 5개 carry-over
            vector<int> left = leftover_after_use(pool, d5);
            st.older5 = left; st.newer5.clear();
        }
        else if(cmd=="SET"){
            // 상대 점수 반영(라운드 가중용)
            string cs, dd; cin >> cs >> dd;
            array<int,5> d{}; int k=0; for(char ch: dd){ if('0'<=ch&&ch<='9'&&k<5) d[k++]=ch-'0'; }
            int ci = parse_cat(cs);
            st.opp_pts += score_category(d,(Cat)ci);
        }
        else if(cmd=="FINISH"){
            break;
        }
        else{
            // ignore
        }
    }
    return 0;
}
//AI 대결 (6승4패)
