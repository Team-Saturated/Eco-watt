// Tab navigation
const tabs = document.querySelectorAll('.tab-btn');
const sections = document.querySelectorAll('.tab-section');
tabs.forEach(btn=>{
  btn.addEventListener('click',()=>{
    tabs.forEach(b=>b.classList.remove('active'));
    sections.forEach(s=>s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
  });
});

// Register checkboxes
const registerGrid = document.getElementById('registerGrid');
for(let i=0;i<10;i++){
  const lbl=document.createElement('label');
  lbl.className='reg-checkbox';
  lbl.innerHTML=`<input type="checkbox" id="reg_${i}" checked/><span>Register ${i}</span>`;
  registerGrid.appendChild(lbl);
}

// Log helpers
function logLine(el,line){
  const p=document.createElement('div');
  p.textContent=line;
  el.prepend(p);
}
const dataTopLog=document.getElementById('dataTopLog');
const fotaLog=document.getElementById('fotaLog');
const cfgLog=document.getElementById('cfgLog');
const writeLog=document.getElementById('writeLog');
const deviceLog=document.getElementById('DeviceLog');

// Chart.js initialization
const charts={};
const ctxIds=['voltageChart','currentChart','frequencyChart','powerChart','pv1VChart','pv1IChart','pv2VChart','pv2IChart','outputPowerChart','temperatureChart'];
ctxIds.forEach(id=>{
  charts[id]=new Chart(document.getElementById(id),{
    type:'line',
    data:{labels:[],datasets:[{label:id,borderColor:'#38bdf8',backgroundColor:'#0ea5e933',data:[],fill:true,tension:0.3}]},
    options:{
      responsive:true,
      plugins:{legend:{display:false}},
      scales:{x:{ticks:{color:'#64748b'}},y:{ticks:{color:'#64748b'}}},
      animation:{duration:500}
    }
  });
});

let chartData={};
ctxIds.forEach(id=>chartData[id]=[]);

// Animate number transitions
function animateNumber(id,value,suffix=''){
  const el=document.getElementById(id);
  const current=parseFloat(el.textContent)||0;
  const start=performance.now();
  const duration=400;
  function step(ts){
    const progress=Math.min((ts-start)/duration,1);
    const val=current+(value-current)*progress;
    el.textContent=val.toFixed(1)+suffix;
    if(progress<1)requestAnimationFrame(step);
  }
  requestAnimationFrame(step);
}
let lastRecordTimestamp = 0; // remember last processed timestamp

async function pollData() {
  try {
    const res = await fetch('/api/data?limit=50');
    const json = await res.json();
    const records = json.records || [];
    if (!records.length) return;

    // Filter only new records (timestamp > lastRecordTimestamp)
    const newRecords = records.filter(r => (r.timestamp || 0) > (lastRecordTimestamp / 1000));
    if (!newRecords.length) return;

    // Process each new record in chronological order
    newRecords.sort((a, b) => a.timestamp - b.timestamp);

    for (const rec of newRecords) {
      const regs = rec.registers || {};
      const recordTimeMs = (rec.timestamp || rec.server_timestamp || 0) * 1000;

      // Extract register values
      const voltage = regs['Vac1_L1_Phase_voltage']?.value || 0;
      const current = regs['Iac1_L1_Phase_current']?.value || 0;
      const frequency = regs['Fac1_L1_Phase_frequency']?.value || 0;
      const pv1V = regs['Vpv1_PV1_input_voltage']?.value || 0;
      const pv1I = regs['Ipv1_PV1_input_current']?.value || 0;
      const pv2V = regs['Vpv2_PV2_input_voltage']?.value || 0;
      const pv2I = regs['Ipv2_PV2_input_current']?.value || 0;
      const temperature = regs['Inverter_internal_temperature']?.value || 0;
      const exportPct = regs['Set_export_power_percentage']?.value || 0;
      const outputPower = regs['Pac_L_Inverter_current_output_power']?.value || 0;
      const power = voltage * current;

      // --- Update dashboard values only for the last one ---
      if (rec === newRecords[newRecords.length - 1]) {
        animateNumber('currentVoltage', voltage);
        animateNumber('currentCurrent', current);
        animateNumber('currentPower', power);
        animateNumber('currentFreq', frequency);

        document.getElementById('pv1Voltage').textContent = pv1V.toFixed(1) + 'V';
        document.getElementById('pv1Current').textContent = pv1I.toFixed(2) + 'A';
        document.getElementById('pv2Voltage').textContent = pv2V.toFixed(1) + 'V';
        document.getElementById('pv2Current').textContent = pv2I.toFixed(2) + 'A';
        document.getElementById('sysTemp').textContent = temperature.toFixed(1);
        document.getElementById('outputPower').textContent = outputPower.toFixed(0);
        document.getElementById('exportRatio').textContent = exportPct.toFixed(0);
        document.getElementById('lastUpdate').textContent = new Date(recordTimeMs).toLocaleTimeString();
      }

      // Format readable time for chart X-axis
      const timeLabel = new Date(recordTimeMs).toLocaleTimeString('en-US', { hour12: false });

      // --- Update all charts for this record ---
      ctxIds.forEach(id => {
        const valMap = {
          voltageChart: voltage,
          currentChart: current,
          frequencyChart: frequency,
          powerChart: power,
          pv1VChart: pv1V,
          pv1IChart: pv1I,
          pv2VChart: pv2V,
          pv2IChart: pv2I,
          outputPowerChart: outputPower,
          temperatureChart: temperature
        };

        const val = valMap[id];
        const chart = charts[id];

        chart.data.labels.push(timeLabel);
        chart.data.datasets[0].data.push(val);

        // Keep only last 10 samples visible
        if (chart.data.labels.length > 10) {
          chart.data.labels.shift();
          chart.data.datasets[0].data.shift();
        }
      });

      lastRecordTimestamp = recordTimeMs;
    }

    // Only update the charts once after processing all new records
    ctxIds.forEach(id => charts[id].update('none'));

  } catch (err) {
    console.error('Poll error:', err);
  }
}

// Poll logs
async function pollLogs(url,container){
  try{
    const r=await fetch(url);
    const j=await r.json();
    container.innerHTML='';
    j.events.forEach(e=>{
      logLine(container,`[${new Date(e.ts).toLocaleTimeString()}] ${e.topic}: ${JSON.stringify(e)}`);
    });
  }catch{}
}
setInterval(()=>pollData(),1500);
setInterval(()=>pollLogs('/api/logs/data',dataTopLog),3000);
setInterval(()=>pollLogs('/api/logs/fota',fotaLog),3000);
setInterval(()=>pollLogs('/api/logs/config',cfgLog),3000);
setInterval(()=>pollLogs('/api/logs/write',writeLog),3000);
setInterval(()=>pollLogs('/api/logs/device',deviceLog),3000);
pollData();

// FOTA
document.getElementById('uploadForm').onsubmit=async e=>{
  e.preventDefault();
  const fw=document.getElementById('fw').files[0];
  const version=document.getElementById('version').value;
  const chunk=document.getElementById('chunk').value;
  const prog=document.getElementById('prog');
  if(!fw){logLine(fotaLog,'⚠ Select firmware');return;}
  prog.style.display='block';
  const fd=new FormData();
  fd.append('firmware',fw);
  fd.append('version',version);
  fd.append('chunk',chunk);
  try{
    const r=await fetch('/api/fota/upload',{method:'POST',body:fd});
    const j=await r.json();
    logLine(fotaLog,'✓ FOTA started: '+JSON.stringify(j));
  }catch(err){logLine(fotaLog,'✗ FOTA failed: '+err);}
};
document.getElementById('rebootBtn').onclick=async()=>{
  const r=await fetch('/api/reboot',{method:'POST'});
  const j=await r.json();
  logLine(fotaLog,'✓ Reboot sent: '+JSON.stringify(j));
};

// Config
function getRegId(){
  let id=0;
  for(let i=0;i<10;i++){
    if(document.getElementById(`reg_${i}`).checked)id|=(1<<i);
  }
  return id;
}
document.getElementById('cfgForm').onsubmit=async e=>{
  e.preventDefault();
  const body={};
  ['poll_period_ms','upload_period_ms','buffer_capacity'].forEach(k=>{
    const v=document.getElementById(k).value;
    if(v!=='')body[k]=Number(v);
  });
  body['reg_req_id_1']=getRegId();
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const j=await r.json();
    logLine(cfgLog,'✓ Config sent: '+JSON.stringify(j.sent));
  }catch(err){logLine(cfgLog,'✗ Config failed: '+err);}
};
document.getElementById('loadCfgBtn').onclick=async()=>{
  const r=await fetch('/api/config');
  const j=await r.json();
  for(const k of ['poll_period_ms','upload_period_ms','buffer_capacity']){
    if(j.config[k]!==undefined)document.getElementById(k).value=j.config[k];
  }
  logLine(cfgLog,'✓ Loaded config');
};

// Write
document.getElementById('writeForm').onsubmit=async e=>{
  e.preventDefault();
  const addr=Number(document.getElementById('wr_address').value);
  const val=Number(document.getElementById('wr_value').value);
  if(isNaN(addr)||isNaN(val)){logLine(writeLog,'⚠ Invalid numbers');return;}
  try{
    const r=await fetch('/api/write',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({address:addr,value:val})});
    const j=await r.json();
    logLine(writeLog,'✓ Write sent: '+JSON.stringify(j.sent));
  }catch(err){logLine(writeLog,'✗ Write failed: '+err);}
};

document.getElementById('writeLoadBtn')
{
  
}

// Device Status log
