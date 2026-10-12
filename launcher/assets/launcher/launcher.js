'use strict';
(() => {
 const $=id=>document.getElementById(id);
 const dictionary={
  ru:{islandLabel:'ТВОЁ ЛЕТО НАЧИНАЕТСЯ ЗДЕСЬ',edition:'XTREME BEACH VOLLEYBALL',play:'ИГРАТЬ',settings:'Настройки',import:'Импорт ISO / ZIP',footer:'ПЛЯЖ. ВОЛЕЙБОЛ. ЛЕТО.',preferences:'ТВОЯ ИГРА',aspect:'Формат экрана',aspectHint:'Пропорции игрового изображения',original:'Исходные пропорции',resolution:'Внутреннее разрешение',resolutionHint:'Качество отрисовки игры',standard:'1× · 720 × 480',higher:'2× · 1440 × 960',highest:'3× · 2160 × 1440',controls:'Назначение кнопок',controlsHint:'Экранное управление и геймпад',previewControls:'Назначение кнопок доступно в APK.',touch:'Экранные кнопки',touchHint:'В режиме «Авто» скрыты с геймпадом',auto:'Авто',show:'Показывать',hide:'Скрывать',swap:'Поменять A/B и X/Y',swapHint:'Перестановка кнопок на геймпаде',language:'Язык',languageHint:'Интерфейс и язык при запуске игры',saved:'Настройки сохранены',done:'Готово',help:'Добро пожаловать',helpImport:'Для первого запуска импортируйте ISO оригинальной USA-версии игры. ZIP с игровыми файлами этой сборки тоже поддерживается.',helpSaves:'При обновлении APK настройки, импортированные файлы и сохранения остаются в приложении. Повторный импорт заменяет игровые файлы и сохраняет прохождение.',helpControls:'L1 — Black, R1 — White, L2/R2 — триггеры Xbox. Кнопки экрана и геймпада можно переназначить. Левый стик также дублирует крестовину.',helpApply:'Во время игры кнопка ⚙ открывает настройки, ✎ — редактор расположения. Формат, разрешение и управление применяются при возвращении в игру. Язык игры меняется при следующем запуске. Во время импорта оставьте приложение открытым.',ready:'Файлы игры установлены · Всё готово',missing:'Для первого запуска импортируйте ISO или ZIP',checking:'Проверка файлов игры…',saveError:'Не удалось сохранить настройки. Повторите попытку.',shortOriginal:'Исходный формат',shortTouch:'Кнопки: ',shortAuto:'авто',shortShow:'вкл.',shortHide:'выкл.',previewImport:'В APK здесь открывается выбор ISO / ZIP.',previewPlay:'Предпросмотр. Запуск игры доступен в APK.',close:'Закрыть',menu:'Меню'},
  en:{islandLabel:'YOUR SUMMER STARTS HERE',edition:'XTREME BEACH VOLLEYBALL',play:'PLAY',settings:'Settings',import:'Import ISO / ZIP',footer:'BEACH. VOLLEYBALL. SUMMER.',preferences:'YOUR GAME',aspect:'Aspect ratio',aspectHint:'Proportions of the game image',original:'Original proportions',resolution:'Internal resolution',resolutionHint:'Game rendering quality',standard:'1× · 720 × 480',higher:'2× · 1440 × 960',highest:'3× · 2160 × 1440',controls:'Button assignments',controlsHint:'Touch controls and gamepad',previewControls:'Button assignments are available in the APK.',touch:'Touch controls',touchHint:'Auto hides controls when a gamepad is connected',auto:'Auto',show:'Show',hide:'Hide',swap:'Swap A/B and X/Y',swapHint:'Switch the buttons on your gamepad',language:'Language',languageHint:'Interface and language when the game starts',saved:'Settings saved',done:'Done',help:'Welcome to the island',helpImport:'For your first launch, import an ISO of the original USA game. The game-data ZIP for this build is also supported.',helpSaves:'Updating the APK retains settings, imported files, and saves. Re-importing replaces game files and keeps your progress.',helpControls:'L1 — Black, R1 — White, L2/R2 — Xbox triggers. Touch and gamepad buttons can be reassigned. The left stick also mirrors the D-pad.',helpApply:'During gameplay, ⚙ opens settings and ✎ edits control positions. Display and control changes apply on return. Game language changes on the next launch. Keep the app open during import.',ready:'Game files installed · Ready to play',missing:'Import an ISO or ZIP to get started',checking:'Checking game files…',saveError:'Could not save settings. Please try again.',shortOriginal:'Original aspect',shortTouch:'Controls: ',shortAuto:'auto',shortShow:'on',shortHide:'off',previewImport:'In the APK, this opens the ISO / ZIP file picker.',previewPlay:'Preview. Launching the game is available in the APK.',close:'Close',menu:'Menu'}
 };
 const bridge=window.AndroidLauncher;
 let state={language:'ru',displayAspect:'source',resolution:0,touchControls:0,swapButtons:false,ready:false,busy:false,statusKind:'checking',message:'',progress:-1};
 let activeOverlay=null,lastFocus=null,toastTimer;
 if(bridge){try{state={...state,...JSON.parse(bridge.getState())}}catch(e){state.statusKind='error';state.message='Не удалось прочитать настройки / Could not read settings'}}
 else {try{state={...state,...JSON.parse(localStorage.getItem('doaxbv-preview-settings')||'{}')}}catch(e){}state.ready=true;state.statusKind='ready';state.message=''}
 const t=key=>dictionary[state.language==='en'?'en':'ru'][key];
 function toast(message){$('toast').textContent=message;$('toast').hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').hidden=true,3400)}
 function render(){
  document.documentElement.lang=state.language==='en'?'en':'ru';
  document.querySelectorAll('[data-i18n]').forEach(el=>el.textContent=t(el.dataset.i18n));
  document.querySelector('nav').setAttribute('aria-label',t('menu'));
  $('helpOpen').setAttribute('aria-label',t('help'));
  for(const id of ['helpClose','settingsClose'])$(id).setAttribute('aria-label',t('close'));
  $('languageLabel').textContent=state.language==='en'?'EN':'RU';
  $('displayAspect').value=state.displayAspect;$('resolution').value=String(state.resolution);$('touchControls').value=String(state.touchControls);$('swapButtons').checked=state.swapButtons;$('language').value=state.language;
  $('play').disabled=!state.ready||state.busy;
  for(const id of ['import','languageQuick','settingsOpen','displayAspect','resolution','touchControls','swapButtons','language','controlsOpen'])$(id).disabled=state.busy;
  const kind=state.busy?'busy':state.statusKind;
  $('statusDot').className='status-dot '+(kind==='ready'?'ready':kind==='error'?'error':kind==='busy'||kind==='checking'?'busy':'');
  $('status').textContent=state.message||t(state.statusKind==='ready'?'ready':state.statusKind==='checking'?'checking':'missing');
  $('progressTrack').hidden=!state.busy||state.progress<0;
  $('progressFill').style.width=Math.min(100,Math.max(0,state.progress))+'%';
  const aspect=state.displayAspect==='source'?t('shortOriginal'):state.displayAspect;
  $('summary').replaceChildren(document.createTextNode(aspect),separator(),document.createTextNode(['720 × 480','1440 × 960','2160 × 1440'][state.resolution]||'720 × 480'),separator(),document.createTextNode(t('shortTouch')+t(['shortAuto','shortShow','shortHide'][state.touchControls]||'shortAuto')));
 }
 function separator(){const el=document.createElement('b');el.textContent='·';return el}
 function save(next){
  const old={...state};state={...state,...next};
  const settings={displayAspect:state.displayAspect,language:state.language,resolution:state.resolution,touchControls:state.touchControls,swapButtons:state.swapButtons};
  let success=false;
  try{if(bridge)success=bridge.saveSettings(JSON.stringify(settings));else{localStorage.setItem('doaxbv-preview-settings',JSON.stringify(settings));success=true}}catch(e){}
  if(!success){state=old;toast(t('saveError'))}
  render();return success;
 }
 function openOverlay(id){if(state.busy)return;lastFocus=document.activeElement;activeOverlay=$(id);activeOverlay.hidden=false;document.querySelector('.app').setAttribute('aria-hidden','true');document.querySelector('.app').inert=true;activeOverlay.querySelector('.close-button').focus()}
 function closeOverlay(){if(!activeOverlay)return false;activeOverlay.hidden=true;activeOverlay=null;document.querySelector('.app').removeAttribute('aria-hidden');document.querySelector('.app').inert=false;if(lastFocus&&lastFocus.isConnected)lastFocus.focus();return true}
 window.handleBack=()=>closeOverlay();
 window.activateGamepadSelection=()=>{
  let el=document.activeElement;
  if(!el||el===document.body||!['BUTTON','SELECT','INPUT'].includes(el.tagName))el=activeOverlay?activeOverlay.querySelector('.close-button'):$(state.ready?'play':'import');
  if(el&&!el.disabled){el.focus();el.click()}
 };
 window.receiveNativeState=next=>{state={...state,...next};render()};
 $('settingsOpen').addEventListener('click',()=>openOverlay('settingsOverlay'));
 $('helpOpen').addEventListener('click',()=>openOverlay('helpOverlay'));
 ['settingsClose','settingsDone','helpClose','helpDone'].forEach(id=>$(id).addEventListener('click',closeOverlay));
 ['settingsOverlay','helpOverlay'].forEach(id=>$(id).addEventListener('click',event=>{if(event.target===$(id))closeOverlay()}));
 ['displayAspect','resolution','touchControls','swapButtons','language'].forEach(key=>$(key).addEventListener('change',()=>save({[key]:key==='swapButtons'?$(key).checked:key==='resolution'||key==='touchControls'?Number($(key).value):$(key).value})));
 $('controlsOpen').addEventListener('click',()=>{if(state.busy||!save({}))return;if(bridge)bridge.openControls();else toast(t('previewControls'))});
 $('languageQuick').addEventListener('click',()=>save({language:state.language==='en'?'ru':'en'}));
 $('play').addEventListener('click',()=>{if(!state.ready||state.busy||!save({}))return;if(bridge)bridge.play();else toast(t('previewPlay'))});
 $('import').addEventListener('click',()=>{if(state.busy||!save({}))return;if(bridge)bridge.importDisc();else toast(t('previewImport'))});
 document.addEventListener('keydown',event=>{
  if(event.key==='Escape'){if(closeOverlay())event.preventDefault();return}
  if(event.key==='Tab'&&activeOverlay){const focusable=Array.from(activeOverlay.querySelectorAll('button,select,input')).filter(el=>!el.disabled);const first=focusable[0],last=focusable[focusable.length-1];if(event.shiftKey&&document.activeElement===first){last.focus();event.preventDefault()}else if(!event.shiftKey&&document.activeElement===last){first.focus();event.preventDefault()}}
  if(['ArrowDown','ArrowUp','ArrowLeft','ArrowRight'].includes(event.key)&&document.activeElement.tagName!=='SELECT'){
   const root=activeOverlay||document.querySelector('.app');const items=Array.from(root.querySelectorAll('button,select,input')).filter(el=>!el.disabled&&el.getClientRects().length);const index=items.indexOf(document.activeElement);const direction=event.key==='ArrowUp'||event.key==='ArrowLeft'?-1:1;if(items.length){items[(index+direction+items.length)%items.length].focus();event.preventDefault()}
  }
 });
 render();
})();
