#include "core/InstrumentArchive.h"
#include "core/JobParameters.h"
#include <iostream>
#include <random>
#include <sstream>
bool parse(std::string s){auto json=a125::instrumentManifest("placeholder",1,{});auto at=json.find("placeholder");json.replace(at,11,s);std::string name;std::array<double,7> v{};return a125::parse125AManifest(json,1,name,v);}
int main(){int cases=0,errors=0;auto check=[&](std::string s,bool expected){++cases;if(parse(s)!=expected){++errors;std::cerr<<"unicode mismatch "<<cases<<'\n';}};
 for(auto s:{"Bass","B\xc3\xa4sse","\xe6\x97\xa5\xe6\x9c\xac","\xd8\xa7\xd9\x84","\xf0\x9f\x8e\xb5","\x7f","\xc2\x80","\xdf\xbf","\xe0\xa0\x80","\xed\x9f\xbf","\xee\x80\x80","\xef\xbf\xbf","\xf0\x90\x80\x80","\xf4\x8f\xbf\xbf","\\uD834\\uDD1E"})check(s,true);
 for(auto s:{"\xff","\x80","\xc0\xaf","\xc1\xbf","\xe0\x80\xaf","\xf0\x80\x80\xaf","\xed\xa0\x80","\xed\xbf\xbf","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xc3","\xe2\x82","\xf0\x9f\x8e","\xc3x","\\uD800","\\uDC00","\\uD800\\u0041"})check(s,false);
 std::mt19937 random(125);int sortCases=0;
 for(int layers=1;layers<=3;++layers)for(int k=0;k<1000;++k){auto p=a125::defaultParameters;p[a125::VelocityLayers]=(layers-1)/2.;std::array<int,3> v;for(int i=0;i<3;++i){v[i]=1+random()%127;p[a125::Velocity1+i]=a125::midiNormalized(v[i]);}std::vector<int> expected(v.begin(),v.begin()+layers);std::sort(expected.begin(),expected.end());auto job=a125::makeJobFromParameters(p,44100.);for(int i=0;i<layers;++i)if(job.velocities[i]!=expected[i])++errors;for(int i=layers;i<3;++i)if(job.velocities[i]!=v[i])++errors;++sortCases;}
 std::cout<<"unicode_cases="<<cases<<" sort_cases="<<sortCases<<" errors="<<errors<<'\n';return errors?1:0;
}
