"""Original short sports Foley. Deterministic synthesis; no external service or samples."""
from pathlib import Path
import wave, math, random, struct
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'ArtSource/Generated/Audio';DEST.mkdir(parents=True,exist_ok=True)
rng=random.Random(260026);rate=24000

def write(name,duration,sample):
    data=[];filtered=0
    for i in range(int(rate*duration)):
        t=i/rate;noise=rng.uniform(-1,1);filtered=filtered*.82+noise*.18
        data.append(sample(t,noise,filtered))
    peak=max(abs(x) for x in data) or 1
    with wave.open(str(DEST/(name+'.wav')),'wb') as w:
        w.setparams((1,2,rate,0,'NONE','not compressed'))
        w.writeframes(b''.join(struct.pack('<h',round(x/peak*22000)) for x in data))
write('runup_step',.14,lambda t,n,f: (.62*math.sin(2*math.pi*(128*t-140*t*t))+.42*f+.12*n)*math.exp(-t*38)*min(1,t/.002))
write('ball_release',.105,lambda t,n,f:(n-f)*math.sin(math.pi*min(1,t/.105))**2*.55)
write('final_ball_pulse',.70,lambda t,n,f: (.65*math.sin(2*math.pi*98*t)+.20*math.sin(2*math.pi*147*t))*math.exp(-t*6)*min(1,t/.02))
print('Generated 3 original Foley/sting sources:',DEST)
