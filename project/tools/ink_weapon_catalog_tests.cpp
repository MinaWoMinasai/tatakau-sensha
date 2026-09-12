// CPU-only catalog tests: compile with WeaponCatalog.cpp and externals include.
#include "../game/ink/WeaponCatalog.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
using namespace ink;
using Json=nlohmann::json;
void Require(bool condition,const char* message) {
    if(!condition) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); }
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    std::ostringstream bytes; bytes<<file.rdbuf(); return bytes.str();
}
void Write(const std::filesystem::path& path,const std::string& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc); file<<bytes;
    Require(file.good(),"test input write");
}
template<class T> void Vary(T& p,const std::vector<TypedFloatField<T>>& floats,const std::vector<TypedIntField<T>>& ints) {
    for(const auto& f:floats) {
        const float original=p.*(f.member);
        const float changed=original==0?0.001f:original*1.05f;
        if(changed>=f.min && changed<=f.max) p.*(f.member)=changed;
    }
    for(const auto& f:ints) {
        int& value=p.*(f.member);
        value=value<f.max?value+1:(std::max)(f.min,value-1);
    }
}
template<class T> void EqualParams(const T& a,const T& b,const std::vector<TypedFloatField<T>>& floats,
    const std::vector<TypedIntField<T>>& ints,const std::vector<TypedBoolField<T>>& bools) {
    for(const auto& f:floats) Require(a.*(f.member)==b.*(f.member),f.key);
    for(const auto& f:ints) Require(a.*(f.member)==b.*(f.member),f.key);
    for(const auto& f:bools) Require(a.*(f.member)==b.*(f.member),f.key);
}
void Equal(const WeaponCatalog& a,const WeaponCatalog& b) {
    Require(a.DefaultId()==b.DefaultId(),"default ID survives");
    Require(a.Entries().size()==b.Entries().size(),"catalog size survives");
    for(size_t i=0;i<a.Entries().size();++i) {
        const auto& x=a.Entries()[i]; const auto& y=b.Entries()[i];
        Require(x.id==y.id && x.displayNameJa==y.displayNameJa && x.reference==y.reference && x.type==y.type,"identity, order and UTF-8 survive");
        if(x.type==WeaponClass::Shooter) EqualParams(x.shooter,y.shooter,ShooterFloatFields(),ShooterIntFields(),ShooterBoolFields());
        else EqualParams(x.stringer,y.stringer,StringerFloatFields(),StringerIntFields(),StringerBoolFields());
    }
}
void Test(const std::filesystem::path& directory) {
    const auto path=directory/std::filesystem::path(L"ブキ保存_日本語.json");
    const auto input=directory/"invalid.json";
    auto catalog=WeaponCatalog::Defaults();
    std::string error;
    Require(catalog.Find("splattershot") && catalog.Find("tri_stringer"),"two known defaults");
    Require(catalog.Find("splattershot")->shooter.repeatFrame==6,"shooter baseline six frames");
    Require(catalog.Find("splattershot")->shooter.inkConsume==0.0092f,"shooter baseline consumption");
    Require(catalog.Find("splattershot")->shooter.inkRecoverStop==20.0f/60.0f,"shooter baseline recovery lock");
    for(const auto& d:catalog.Entries()) Require(ValidateWeapon(d,error),error.c_str());
    const std::string copy1=catalog.Clone("splattershot","試作シューター",error);
    const std::string copy2=catalog.Clone("splattershot","試作シューター",error);
    Require(!copy1.empty() && !copy2.empty() && copy1!=copy2,"clone IDs distinct despite same display name");
    auto shooter=*catalog.Find(copy1); Vary(shooter.shooter,ShooterFloatFields(),ShooterIntFields());
    Require(catalog.Upsert(shooter,error),error.c_str());
    auto stringer=*catalog.Find("tri_stringer"); Vary(stringer.stringer,StringerFloatFields(),StringerIntFields());
    stringer.stringer.explosiveAtMidCharge=false;
    Require(catalog.Upsert(stringer,error),error.c_str());
    Require(catalog.Save(path,error),error.c_str());
    WeaponCatalog loaded;
    Require(loaded.Load(path,error),error.c_str()); Equal(catalog,loaded);
    auto changed=*catalog.Find(copy2); changed.displayNameJa="置換して保存";
    Require(catalog.Upsert(changed,error),error.c_str());
    Require(catalog.Save(path,error),"atomic replacement of existing file");
    Require(loaded.Load(path,error),error.c_str()); Equal(catalog,loaded);
    const auto before=loaded;
    const Json valid=Json::parse(Read(path));
    auto invalid=[&](const Json& json,const char* label) {
        Write(input,json.dump()); Require(!loaded.Load(input,error),label);
        Require(!error.empty(),"failure reports a message"); Equal(before,loaded);
    };
    auto bad=valid; bad["schemaVersion"]=2; invalid(bad,"unknown schema rejected");
    bad=valid; bad["schemaVersion"]=1.0; invalid(bad,"noninteger schema rejected");
    bad=valid; bad["weapons"][0]["class"]="charger"; invalid(bad,"unknown class rejected");
    bad=valid; bad["weapons"].push_back(bad["weapons"][0]); invalid(bad,"duplicate IDs rejected");
    bad=valid; bad["weapons"][0]["id"]="Bad/Path"; invalid(bad,"invalid stable ID rejected");
    bad=valid; bad["weapons"][0]["displayNameJa"]=""; invalid(bad,"empty name rejected");
    bad=valid; bad["defaultWeaponId"]="missing"; invalid(bad,"missing default rejected");
    bad=valid; bad["weapons"]=Json::array(); invalid(bad,"empty catalog rejected");
    bad=valid; bad["weapons"][0]["params"]["repeatFrame"]=-1; invalid(bad,"negative duration rejected");
    bad=valid; bad["weapons"][0]["params"]["repeatFrame"]="6"; invalid(bad,"numeric string rejected");
    bad=valid; bad["weapons"][0]["params"]["repeatFrame"]=nullptr; invalid(bad,"null numeric rejected");
    bad=valid; bad["weapons"][0]["params"]["repeatFrame"]=1.0e100; invalid(bad,"oversize float rejected before cast");
    bad=valid; bad["weapons"][0]["params"]["paintDropletCount"]=2.5; invalid(bad,"fractional count rejected");
    bad=valid; bad["weapons"][0]["params"]["paintDropletCount"]=std::numeric_limits<uint64_t>::max(); invalid(bad,"oversize integer rejected before cast");
    bad=valid; bad["weapons"][0]["params"]["minimumDamage"]=100; invalid(bad,"damage order rejected");
    bad=valid; bad["weapons"][0]["params"]["repeatFrmae"]=6; invalid(bad,"misspelled field rejected");
    bad=valid; bad["weapons"][1]["params"]["midChargeTime"]=0.001f; invalid(bad,"invalid charge order rejected");
    bad=valid; bad["weapons"][1]["params"]["enableChargeKeep"]=true; invalid(bad,"unsupported charge keep rejected");
    bad=valid; bad["weapons"][1]["params"]["arrowCount"]=4; invalid(bad,"unsupported arrow count rejected");
    Write(input,"{broken"); Require(!loaded.Load(input,error),"malformed JSON rejected"); Equal(before,loaded);
    Write(input,"{\"schemaVersion\":1,\"schemaVersion\":1,\"defaultWeaponId\":\"x\",\"weapons\":[]}");
    Require(!loaded.Load(input,error) && error.find("重複キー")!=std::string::npos,"duplicate JSON key explicitly rejected"); Equal(before,loaded);
    Require(!loaded.Load(directory/"missing.json",error),"missing file rejected"); Equal(before,loaded);
    changed=*loaded.Find("splattershot"); changed.shooter.baseDamage=std::numeric_limits<float>::quiet_NaN();
    Require(!loaded.Upsert(changed,error),"NaN UI input rejected"); Equal(before,loaded);
    Require(loaded.Clone("missing","名前",error).empty(),"unknown clone source rejected"); Equal(before,loaded);
    const auto bytesBefore=Read(path);
#ifdef _WIN32
    HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Require(locked!=INVALID_HANDLE_VALUE,"test locks existing destination against replacement");
    Require(!loaded.Save(path,error),"locked destination save fails"); CloseHandle(locked);
    Require(Read(path)==bytesBefore,"save failure preserves original bytes"); Equal(before,loaded);
#endif
    bad=valid; bad["weapons"][0]["params"]=Json::object();
    Write(input,bad.dump()); Require(loaded.Load(input,error),"omitted optional params use class defaults");
    Require(loaded.Find("splattershot")->shooter.repeatFrame==6,"missing field uses default, not old active value");
    bad["weapons"][0]["params"]["paintDropletSpacing"]=0.1;
    bad["weapons"][0]["params"]["playerHitRadius"]=0.001;
    Write(input,bad.dump()); Require(loaded.Load(input,error),"decimal lower bounds compare in float destination type");
    std::cout<<"PASS: typed field round trip, UTF-8 paths, clone IDs, validation transactions, atomic replacement/failure\n";
}
} // namespace

int main(int argc,char** argv) {
    if(argc==3 && std::string(argv[1])=="--write-defaults") {
        std::string error;
        if(!ink::WeaponCatalog::Defaults().Save(std::filesystem::u8path(argv[2]),error)) { std::cerr<<error<<'\n'; return 1; }
        return 0;
    }
    const auto directory=std::filesystem::current_path()/"ink_catalog_test_data";
    std::filesystem::create_directories(directory);
    Test(directory);
    return 0;
}
