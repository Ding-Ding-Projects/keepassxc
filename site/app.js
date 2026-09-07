import '@material/web/all.js';

const $ = (selector) => document.querySelector(selector);
const panels = ['overview', 'downloads', 'docs', 'changelog', 'settings'];
const english = new Map([...document.querySelectorAll('[data-copy]')].map(element => [element.dataset.copy, element.textContent]));
const cantonese = {
  platform:'Windows x64 · 開放原始碼',overview:'概覽',downloads:'下載',documentation:'使用文件',changelog:'更新記錄',settings:'設定',
  localFirst:'資料庫留喺你手上',headline:'熟悉嘅密碼庫，全新工作空間。',intro:'專為 Windows 改建嘅 KeePassXC 分支，採用 Material 介面。KDBX 資料庫留喺本機，桌面體驗持續完善。',
  downloadWindows:'下載 Windows 版本',readDocs:'閱讀使用文件',verification:'驗證狀態',workInProgress:'介面重建仍然進行中。',honestStatus:'拖曳、自動更新同完整功能清單仍然修正及驗證緊。有版本可以下載，唔代表呢批工作已經完成。',followProgress:'查看修正進度',
  vault:'密碼庫',vaultText:'喺同一個桌面工作空間處理群組、項目、搜尋同項目詳情。',appearance:'外觀',appearanceText:'喺桌面設定試用明暗主題、種子色、密度同字體。',history:'歷史同報告',historyText:'查看資料庫資訊同已記錄嘅歷史。實際行為同驗證限制請參閱文件。',
  downloadDescription:'最近記錄嘅 Windows x64 穩定套件，同埋對應嘅發佈來源資料。',downloadInstaller:'下載 Setup.exe',releaseNotes:'版本說明',unsignedTitle:'未簽署安裝程式',unsignedText:'Squirrel.Windows 安裝程式未經簽署。Windows 可能顯示未知發行者或 SmartScreen 提示。套件雜湊只用於完整性檢查，唔代表發行者身份已獲認證。',updateConsent:'安裝更新要保留未儲存工作，並等你明確選擇重新啟動。今次修正嘅完整更新流程仍然待驗證。',
  docsIntro:'閱讀持續維護嘅文章，了解行為、設定同驗證證據。',buildTitle:'喺 Windows 建置',buildText:'使用已提交嘅包裝指令取得指定工具鏈並建置。呢條指令只負責建置，唔會安裝或啟動程式。',buildDetails:'建置同套件詳情',
  changelogIntro:'閱讀已記錄嘅修改同有日期嘅版本。呢頁仲未實作完整本機版本歷史瀏覽。',recordedChanges:'已記錄修改',allReleases:'所有版本',settingsIntro:'呢啲偏好只留喺呢個瀏覽器，唔會更改桌面安裝。',darkTheme:'深色主題',footer:'KeePassXC Material 係獨立分支。官方項目請瀏覽 keepassxc.org。',reportIssue:'回報問題',
  regexTitle:'文件搜尋運算式',regexIntro:'使用 JavaScript 正規運算式，最多 300 個字元，喺獨立 worker 執行，時限為 250 毫秒。',plainSearch:'改用純文字',cancel:'取消',apply:'套用',
};
const extra = {
  unavailable:['Version and timestamp unavailable.','版本同時間未能取得。'],
  website:['Website','網頁'],updated:['Updated','更新時間'],release:['Released version','已發佈版本'],source:['Source commit','來源 commit'],packageHash:['Package SHA-256','套件 SHA-256'],installerSize:['Installer bytes','安裝程式位元組'],
  search:['Search documentation','搜尋使用文件'],language:['Language','語言'],pattern:['Pattern','模式'],flags:['Flags','旗標'],plain:['Plain text','純文字'],matches:['matching articles','篇符合文章'],noMatch:['No matching articles.','冇符合嘅文章。'],
  searchFailed:['The expression is invalid or exceeded its time limit.','運算式無效或者超過時限。'],preferenceFailed:['Browser storage is unavailable. Changes apply for this visit only.','瀏覽器儲存空間未能使用，修改只會喺今次瀏覽生效。'],
};
let state = {language:'en',dark:matchMedia('(prefers-color-scheme: dark)').matches,panel:0};
try { const saved=JSON.parse(localStorage.getItem('kpxc.material.site.v1')||'null'); if(saved&&['en','yue','both'].includes(saved.language)&&typeof saved.dark==='boolean'&&Number.isInteger(saved.panel)&&saved.panel>=0&&saved.panel<panels.length)state=saved; } catch {}
let releaseData=null, buildData=null, regex=null, worker=null, timer=null;
const text=(key)=>{const pair=extra[key]||[english.get(key)||key,cantonese[key]||english.get(key)||key];return state.language==='both'?`${pair[0]} · ${pair[1]}`:pair[state.language==='yue'?1:0];};
const save=()=>{try{localStorage.setItem('kpxc.material.site.v1',JSON.stringify(state));}catch{$('#preference-status').textContent=text('preferenceFailed');}};
const localTime=(utc)=>new Intl.DateTimeFormat(state.language==='yue'?'zh-HK':'en-CA',{dateStyle:'medium',timeStyle:'long'}).format(new Date(utc));
function renderProvenance(){
  $('#build-status').textContent=buildData?`${text('website')} ${buildData.version} · ${text('updated')} ${localTime(buildData.updatedAtUtc)}`:text('unavailable');
  document.querySelectorAll('[data-release]').forEach(element=>element.textContent=releaseData?`${text('release')} ${releaseData.version} · ${text('updated')} ${localTime(releaseData.updatedAtUtc)}`:text('unavailable'));
  const details=$('#release-details');details.replaceChildren();
  if(releaseData)for(const [label,value] of [['source',releaseData.sourceCommit],['installerSize',String(releaseData.installer.bytes)],['packageHash',releaseData.package.sha256]]){const term=document.createElement('dt'),definition=document.createElement('dd');term.textContent=text(label);definition.textContent=value;details.append(term,definition);}
}
function renderLanguage(){
  document.documentElement.lang=state.language==='yue'?'yue-Hant':'en';
  document.querySelectorAll('[data-copy]').forEach(element=>element.textContent=text(element.dataset.copy));
  $('#doc-search').label=text('search');$('#language').label=text('language');$('#regex-pattern').label=text('pattern');$('#regex-flags').label=text('flags');$('#theme-switch').ariaLabel=text('darkTheme');
  renderProvenance();searchDocs();
}
function selectPanel(index,focus=false){state.panel=index;$('#navigation').activeTabIndex=index;panels.forEach((id,i)=>$('#'+id).hidden=i!==index);save();if(focus)$('#'+panels[index]).querySelector('h2')?.scrollIntoView({block:'nearest'});}
$('#navigation').addEventListener('change',()=>selectPanel($('#navigation').activeTabIndex));
$('#show-docs').addEventListener('click',()=>selectPanel(2,true));
$('#language').value=state.language;$('#language').addEventListener('change',()=>{state.language=$('#language').value;save();renderLanguage();});
function theme(){document.documentElement.dataset.theme=state.dark?'dark':'light';$('#theme-switch').selected=state.dark;}
$('#theme-switch').addEventListener('change',()=>{state.dark=$('#theme-switch').selected;theme();save();});
const articles=[
  ['Automatic updates','自動更新','delivery/auto-updates.md'],['Squirrel.Windows installer','Squirrel.Windows 安裝程式','delivery/squirrel-installer.md'],['Build scripts','建置指令','delivery/build-scripts.md'],['Website release provenance','網頁版本來源','delivery/website-release-provenance.md'],['Window title bar','視窗標題列','design/frameless-title-bar.md'],['Tabs and navigation','分頁同導覽','navigation/tabs.md'],['Appearance customization','自訂外觀','design/material-3-appearance.md'],['Local history','本機歷史','records/local-history.md'],['Language modes','語言模式','messaging/language-modes.md'],['Regex workbench','正規運算式工作台','search/regex-builder.md'],
];
const articleLabel=(article)=>state.language==='both'?`${article[0]} · ${article[1]}`:article[state.language==='yue'?1:0];
function renderArticles(indices){const list=$('#doc-list');list.replaceChildren();for(const index of indices){const button=document.createElement('md-outlined-button');button.href='https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/'+articles[index][2];button.textContent=articleLabel(articles[index]);list.append(button);}$('#search-status').textContent=indices.length?`${indices.length} ${text('matches')} · ${regex?'Regex':text('plain')}`:text('noMatch');}
function searchDocs(){
  clearTimeout(timer);if(worker){worker.terminate();worker=null;}
  const query=$('#doc-search').value||'';
  if(!regex){renderArticles(articles.flatMap((article,index)=>articleLabel(article).toLocaleLowerCase().includes(query.toLocaleLowerCase())?[index]:[]));return;}
  const active=new Worker(new URL('./search-worker.js',import.meta.url),{type:'module'});worker=active;
  timer=setTimeout(()=>{active.terminate();if(worker===active){worker=null;$('#search-status').textContent=text('searchFailed');}},250);
  active.onmessage=event=>{if(worker!==active)return;clearTimeout(timer);active.terminate();worker=null;if(event.data.error){$('#search-status').textContent=text('searchFailed');return;}renderArticles(event.data.indices);};
  active.onerror=()=>{clearTimeout(timer);active.terminate();if(worker===active){worker=null;$('#search-status').textContent=text('searchFailed');}};
  active.postMessage({pattern:query,flags:regex.flags,items:articles.map(articleLabel)});
}
$('#doc-search').addEventListener('input',searchDocs);
$('#regex-open').addEventListener('click',()=>{$('#regex-pattern').value=$('#doc-search').value||'';$('#regex-flags').value=regex?.flags??'i';$('#regex-error').textContent='';$('#regex-dialog').show();});
$('#regex-cancel').addEventListener('click',()=>$('#regex-dialog').close());
$('#regex-clear').addEventListener('click',()=>{regex=null;$('#regex-dialog').close();searchDocs();});
$('#regex-apply').addEventListener('click',()=>{const pattern=$('#regex-pattern').value||'';if(pattern.length>300){$('#regex-error').textContent=text('searchFailed');return;}regex={flags:$('#regex-flags').value};$('#doc-search').value=pattern;$('#regex-dialog').close();searchDocs();});
async function readJson(path){const response=await fetch(path,{cache:'no-cache'});if(!response.ok)throw Error('Metadata unavailable');const reader=response.body.getReader();const chunks=[];let total=0;while(true){const {done,value}=await reader.read();if(done)break;total+=value.byteLength;if(total>32768){await reader.cancel();throw Error('Metadata too large');}chunks.push(value);}const bytes=new Uint8Array(total);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}return JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));}
const validTime=value=>typeof value==='string'&&Number.isFinite(Date.parse(value));
readJson('./build-provenance.json').then(data=>{if(data.schemaVersion!==1||!/^\d+\.\d+\.\d+$/.test(data.version)||!validTime(data.updatedAtUtc)||!/^[a-f0-9]{40}$/.test(data.sourceCommit))throw Error('Invalid build metadata');buildData=data;renderProvenance();}).catch(()=>renderProvenance());
readJson('./release.json').then(data=>{const base=`https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v${data.version}/`;if(data.schemaVersion!==1||!/^\d+\.\d+\.\d+$/.test(data.version)||data.unsigned!==true||!validTime(data.updatedAtUtc)||data.installer?.url!==base+'Setup.exe'||!Number.isSafeInteger(data.installer.bytes)||data.installer.bytes<=0||!/^[a-f0-9]{40}$/i.test(data.sourceCommit)||!/^[a-f0-9]{64}$/i.test(data.package?.sha256)||data.notesUrl!==`https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v${data.version}`)throw Error('Invalid release metadata');releaseData=data;document.querySelectorAll('.installer').forEach(button=>{button.href=data.installer.url;button.disabled=false;});$('#release-notes').href=data.notesUrl;$('#release-notes').disabled=false;renderProvenance();}).catch(()=>renderProvenance());
theme();selectPanel(state.panel);renderLanguage();
