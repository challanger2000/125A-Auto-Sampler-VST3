from pathlib import Path
import shutil,subprocess,os,json,concurrent.futures
repo=Path('/workspace/125A-Auto-Sampler-VST3');out=Path('/workspace/autosampler-reaudit-f7d2/mutants');out.mkdir(exist_ok=True)
changes={
 'A04_slots16':('BuiltInSampler.h','if(owned_.size()>=maxVoices+2)return false;','static unsigned auditPublications=0; if(++auditPublications>16)return false;'),
 'A06_ignore_flush':('SampleProcessing.h','if(!out)throw std::runtime_error("WAV write/flush failed");','/* test-only: ignore flush failure */'),
 'A07_ignore_reset':('BuiltInSampler.h','clearVoices_.store(true,std::memory_order_release);','/* test-only: ignore reset */'),
 'A08_voice_count':('BuiltInSampler.h','std::max(1.f,envelopeSum)','std::max(1.f,static_cast<float>(activeVoices))'),
 'A10_version10':('ManifestJson.h','(field==1 && value!=1)','(field==1 && value!=1 && value!=10)'),
 'A14_pitch_only':('BuiltInSampler.h','!(noteId>=0 ? v.noteId==noteId : v.key==key)','!(v.key==key)'),
 'A15_device':('InstrumentArchive.h','stem.insert(dot==std::string::npos?stem.size():dot,"_");','/* test-only: leave device name unchanged */'),
}
changes.update({
 'R02_no_decoder_budget':('BuiltInSampler.h','if(decodedBytes>remainingDecodedBytes)return false;','/* isolated negative: skip decoder budget */'),
 'R03_keep_old_gain':('BuiltInSampler.h','            polyMixGain_=1.f;','            /* isolated negative: retain old gain */'),
 'R04_invalid_unicode':('ManifestJson.h',"if(ch=='\"')return validUtf8(out);","if(ch=='\"')return true;"),
 'R05_keep_rr_epoch':('BuiltInSampler.h','if(++rrEpoch_==0){','if(false){'),
})
def run(item):
 name,(file,old,new)=item;root=out/name;shutil.copytree(repo/'src/core',root/'src/core',dirs_exist_ok=True)
 p=root/'src/core'/file;s=p.read_text();assert s.count(old)==1;(p).write_text(s.replace(old,new))
 tmp=root/'tmp';tmp.mkdir(exist_ok=True)
 cmd=['g++','-std=c++23','-O3','-DNDEBUG','-I',str(root/'src'),str(repo/'tests/core_tests.cpp'),'-o',str(root/'test'),'-pthread']
 c=subprocess.run(cmd,capture_output=True,text=True);(root/'compile.log').write_text(c.stdout+c.stderr)
 if c.returncode:return {'mutant':name,'compile':c.returncode,'result':'INVALID_BUILD'}
 t=subprocess.run([str(root/'test')],capture_output=True,text=True,env={**os.environ,'TMPDIR':str(tmp)},cwd=root)
 (root/'test.log').write_text(t.stdout+t.stderr)
 return {'mutant':name,'compile':0,'exit':t.returncode,'result':'KILLED' if t.returncode else 'SURVIVED','diagnostic':t.stderr.strip()}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:results=list(ex.map(run,changes.items()))
(out/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
