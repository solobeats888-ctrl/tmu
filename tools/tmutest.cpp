// Herramienta de prueba (no va en el plugin): escanea, clasifica y renderiza presets a WAV.
#define TSF_IMPLEMENTATION
#include "tsf.h"
#include "TMUCore.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <map>
using namespace tmu;
static void writeWav(const char* path, const std::vector<float>& st, int sr){
  FILE* f=fopen(path,"wb"); int n=(int)st.size(); int bytes=n*2;
  auto w32=[&](uint32_t x){fwrite(&x,4,1,f);}; auto w16=[&](uint16_t x){fwrite(&x,2,1,f);};
  fwrite("RIFF",1,4,f); w32(36+bytes); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(2); w32(sr); w32(sr*4); w16(4); w16(16);
  fwrite("data",1,4,f); w32(bytes); for(float v:st){ v=v>1?1:v<-1?-1:v; int16_t s=(int16_t)(v*32767); fwrite(&s,2,1,f);} fclose(f);}
int main(int argc,char**argv){
  if(argc>=3 && !strcmp(argv[1],"list")){ // list <file>: imprime tsv
    for(int a=2;a<argc;a++){ std::vector<RawPreset> ps; if(!readPresetList(argv[a],ps)){printf("ERR\t%s\n",argv[a]);continue;}
      auto cats=categorizeFile(ps);
      for(size_t i=0;i<ps.size();i++) printf("%s\t%d\t%d\t%d\t%d\t%s\t%s\n",argv[a],ps[i].phdrIndex,ps[i].bank,ps[i].program,isJunkName(ps[i].name)?-1:cats[i],categoryName(cats[i]),ps[i].name.c_str()); }
    return 0; }
  if(argc>=5 && !strcmp(argv[1],"render")){ // render <file> <idx> <out.wav> [drum]
    std::vector<uint8_t> buf; std::string err;
    if(!extractPreset(argv[2],atoi(argv[3]),buf,err)){printf("EXTRACT_FAIL %s\n",err.c_str());return 1;}
    tsf* t=tsf_load_memory(buf.data(),(int)buf.size()); if(!t){printf("TSF_FAIL size=%zu\n",buf.size());return 2;}
    int sr=44100; tsf_set_output(t,TSF_STEREO_INTERLEAVED,sr,0); tsf_set_max_voices(t,64); tsf_channel_set_presetindex(t,0,0);
    std::vector<float> out; std::vector<float> blk(512*2);
    int notes[]={48,55,60,64,67,72}; bool drum=argc>5;
    int drumNotes[]={36,38,42,46,49,39};
    for(int k=0;k<6;k++){ int n=drum?drumNotes[k]:notes[k]; tsf_channel_note_on(t,0,n,0.8f);
      for(int b=0;b<sr*0.4/512;b++){ tsf_render_float(t,blk.data(),512,0); out.insert(out.end(),blk.begin(),blk.end()); }
      tsf_channel_note_off(t,0,n);
      for(int b=0;b<sr*0.15/512;b++){ tsf_render_float(t,blk.data(),512,0); out.insert(out.end(),blk.begin(),blk.end()); } }
    double sum=0, pk=0; for(float v:out){sum+=v*v; pk=std::max(pk,(double)fabs(v));}
    printf("OK bytes=%zu presetName=%s rms=%.4f peak=%.3f\n",buf.size(),tsf_get_presetname(t,0),sqrt(sum/out.size()),pk);
    writeWav(argv[4],out,sr); tsf_close(t); return 0; }
  return 3; }
