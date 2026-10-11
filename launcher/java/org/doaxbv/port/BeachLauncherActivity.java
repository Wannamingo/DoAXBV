package org.doaxbv.port;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.view.View;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import java.io.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicInteger;
import org.json.JSONObject;

/** A local, offline presentation layer over the original game and disc importer. */
public final class BeachLauncherActivity extends Activity {
    private static final String PAGE="file:///android_asset/launcher/index.html";
    private static final String XBE_HASH="053d44e885fa33c1d15d909a533f39dfbd976e97eeaf67e4fdef8438ea7e5c54";
    private WebView web;
    private SharedPreferences preferences;
    private volatile boolean importing,ready;
    private volatile String statusKind="checking",message="";
    private volatile int progress=-1;
    private boolean pageLoaded,destroyed;
    private int importOrientation;
    private final AtomicInteger refreshGeneration=new AtomicInteger();

    private boolean english(){return "en".equals(preferences.getString("language","ru"));}
    private String text(String ru,String en){return english()?en:ru;}
    private File disc(){return new File(getFilesDir(),"disc");}

    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        preferences=getSharedPreferences("controls",MODE_PRIVATE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().setNavigationBarColor(Color.rgb(5,31,46));
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        try{
            web=new WebView(this);
            web.setBackgroundColor(Color.rgb(9,43,61));
            WebSettings settings=web.getSettings();
            settings.setJavaScriptEnabled(true);
            settings.setDomStorageEnabled(false);
            settings.setAllowFileAccess(false); // android_asset remains accessible.
            settings.setAllowContentAccess(false);
            settings.setAllowFileAccessFromFileURLs(false);
            settings.setAllowUniversalAccessFromFileURLs(false);
            settings.setBlockNetworkLoads(true);
            settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
            settings.setSupportZoom(false);
            settings.setUseWideViewPort(true);
            settings.setLoadWithOverviewMode(true);
            web.addJavascriptInterface(new LauncherBridge(),"AndroidLauncher");
            web.setWebViewClient(new WebViewClient(){
                @Override public boolean shouldOverrideUrlLoading(WebView view,WebResourceRequest request){return true;}
                @Override public boolean shouldOverrideUrlLoading(WebView view,String url){return true;}
                @Override public void onPageFinished(WebView view,String url){
                    if(PAGE.equals(url)){pageLoaded=true;pushState();}
                }
            });
            setContentView(web);
            web.loadUrl(PAGE);
        }catch(RuntimeException unavailable){showFallback();}
    }

    private void showFallback(){
        LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);
        int padding=(int)(24*getResources().getDisplayMetrics().density);layout.setPadding(padding,padding,padding,padding);
        TextView title=new TextView(this);title.setText("Dead or Alive Xtreme Beach Volleyball");title.setTextSize(24);layout.addView(title);
        TextView explanation=new TextView(this);explanation.setText(text("Обновите Android System WebView для нового оформления. Все настройки доступны в стандартном меню.","Update Android System WebView for the new interface. All settings are available in the standard menu."));layout.addView(explanation);
        Button original=new Button(this);original.setText(text("Открыть стандартное меню","Open standard menu"));
        original.setOnClickListener(v->startActivity(new Intent(this,LauncherActivity.class)));layout.addView(original);setContentView(layout);
    }

    private JSONObject state(){
        JSONObject state=new JSONObject();
        try{
            state.put("language",english()?"en":"ru");
            state.put("displayAspect",LauncherActivity.savedAspect(preferences));
            state.put("resolution",Math.max(0,Math.min(1,preferences.getInt("resolution",0))));
            state.put("touchControls",Math.max(0,Math.min(2,preferences.getInt("touchControls",0))));
            state.put("swapButtons",preferences.getBoolean("swapButtons",false));
            state.put("ready",ready);state.put("busy",importing);
            state.put("statusKind",statusKind);state.put("message",message);state.put("progress",progress);
        }catch(Exception ignored){}
        return state;
    }
    private void pushState(){
        if(!destroyed&&pageLoaded&&web!=null)web.evaluateJavascript("window.receiveNativeState&&window.receiveNativeState("+state()+");",null);
    }
    private void fail(String message){this.message=message;statusKind="error";pushState();}

    private final class LauncherBridge {
        @JavascriptInterface public String getState(){return state().toString();}
        @JavascriptInterface public boolean saveSettings(String json){
            if(importing||json==null||json.length()>2048)return false;
            try{
                JSONObject values=new JSONObject(json);
                String aspect=values.getString("displayAspect"),language=values.getString("language");
                int resolution=values.getInt("resolution"),touch=values.getInt("touchControls");
                boolean swap=values.getBoolean("swapButtons");
                if(!BeachSettings.valid(aspect,language,resolution,touch))return false;
                // Keep the existing preference file, names, types, and unrelated entries.
                boolean saved=preferences.edit().putString("displayAspect",aspect).putString("language",language)
                    .putInt("resolution",resolution).putInt("touchControls",touch).putBoolean("swapButtons",swap).commit();
                if(saved)runOnUiThread(()->pushState());
                return saved;
            }catch(Exception invalid){return false;}
        }
        @JavascriptInterface public void play(){runOnUiThread(()->{
            if(importing||!ready)return;
            try{startActivity(new Intent(BeachLauncherActivity.this,GameActivity.class));}
            catch(RuntimeException error){fail(text("Не удалось запустить игру: ","Could not start the game: ")+error.getMessage());}
        });}
        @JavascriptInterface public void importDisc(){runOnUiThread(()->{
            if(importing)return;
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.setType("*/*");intent.addCategory(Intent.CATEGORY_OPENABLE);
            try{startActivityForResult(intent,1);}
            catch(ActivityNotFoundException error){fail(text("Не найден системный выбор файлов. Включите приложение «Файлы».","System file picker unavailable. Enable the Files application."));}
        });}
    }

    @Override protected void onResume(){super.onResume();if(preferences!=null&&!importing)refresh();}
    private void refresh(){
        final int generation=refreshGeneration.incrementAndGet();
        new Thread(()->{
            boolean found=false;
            try{DiscImporter.recover(getFilesDir());found=new File(disc(),".verified-usa").isFile()
                &&XBE_HASH.equals(DiscImporter.hash(new File(disc(),"default.xbe")))&&new File(disc(),"datadvd.afs").isFile();}
            catch(Exception ignored){}
            final boolean verified=found;
            runOnUiThread(()->{if(!destroyed&&!importing&&generation==refreshGeneration.get()){
                ready=verified;statusKind=ready?"ready":"missing";message="";progress=-1;pushState();
            }});
        },"doaxbv-launcher-check").start();
    }
    @Override public void onBackPressed(){
        if(importing){Toast.makeText(this,text("Дождитесь завершения импорта","Wait for the import to finish"),Toast.LENGTH_SHORT).show();return;}
        if(pageLoaded&&web!=null)web.evaluateJavascript("window.handleBack&&window.handleBack()",result->{if(!"true".equals(result))finish();});
        else super.onBackPressed();
    }
    @Override public boolean onKeyDown(int keyCode,KeyEvent event){
        if(pageLoaded&&web!=null){
            if(keyCode==KeyEvent.KEYCODE_BUTTON_B){onBackPressed();return true;}
            if(keyCode==KeyEvent.KEYCODE_BUTTON_A||keyCode==KeyEvent.KEYCODE_BUTTON_START){
                if(event.getRepeatCount()==0)web.evaluateJavascript("window.activateGamepadSelection&&window.activateGamepadSelection()",null);
                return true;
            }
        }
        return super.onKeyDown(keyCode,event);
    }
    @Override protected void onDestroy(){
        destroyed=true;refreshGeneration.incrementAndGet();pageLoaded=false;
        if(web!=null){web.removeJavascriptInterface("AndroidLauncher");web.destroy();web=null;}
        super.onDestroy();
    }

    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(request!=1||result!=RESULT_OK||data==null||importing)return;
        Uri uri=data.getData();if(uri==null)return;
        importing=true;refreshGeneration.incrementAndGet();progress=0;statusKind="busy";
        message=text("Чтение образа и проверка игровых файлов…","Reading the image and checking game files…");pushState();
        importOrientation=getRequestedOrientation();setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        new Thread(()->{
            String resultText;boolean succeeded;
            try{importGame(uri);resultText=text("Импорт завершён · Можно играть","Import complete · Ready to play");succeeded=true;}
            catch(Exception error){resultText=text("Не удалось импортировать: ","Import failed: ")+error.getMessage();succeeded=false;}
            final String completed=resultText;final boolean success=succeeded;
            boolean installed=false;
            try{DiscImporter.recover(getFilesDir());installed=new File(disc(),".verified-usa").isFile()
                &&XBE_HASH.equals(DiscImporter.hash(new File(disc(),"default.xbe")))&&new File(disc(),"datadvd.afs").isFile();}catch(Exception ignored){}
            final boolean verified=installed;
            runOnUiThread(()->{
                importing=false;ready=verified;progress=-1;statusKind=success?"ready":"error";message=completed;
                if(!destroyed){getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);setRequestedOrientation(importOrientation);pushState();}
            });
        },"doaxbv-import").start();
    }

    // Uses the original transactional importer unchanged, including identity checks and recovery.
    private void importGame(Uri uri)throws Exception{
        android.app.ActivityManager manager=(android.app.ActivityManager)getSystemService(ACTIVITY_SERVICE);
        List<android.app.ActivityManager.RunningAppProcessInfo> processes=manager.getRunningAppProcesses();
        if(processes!=null)for(android.app.ActivityManager.RunningAppProcessInfo process:processes)
            if((getPackageName()+":game").equals(process.processName))throw new IOException(text("Сначала выйдите из игры через Select","Exit the game using Select first"));
        Map<String,DiscImporter.Expected> expected=new HashMap<>();
        try(InputStream input=getAssets().open("disc-identity.json")){
            ByteArrayOutputStream bytes=new ByteArrayOutputStream();byte[] buffer=new byte[4096];int n;
            while((n=input.read(buffer))!=-1)bytes.write(buffer,0,n);
            JSONObject files=new JSONObject(bytes.toString("UTF-8"));Iterator<String> names=files.keys();
            while(names.hasNext()){String name=names.next();JSONObject file=files.getJSONObject(name);
                expected.put(name,new DiscImporter.Expected(file.getLong("size"),file.getString("sha256")));}
        }
        boolean zip;
        try(InputStream input=getContentResolver().openInputStream(uri)){
            if(input==null)throw new IOException(text("Не удалось открыть файл","Could not open the file"));
            zip=input.read()=='P'&&input.read()=='K';
        }
        final long total=expected.values().stream().mapToLong(file->file.size).sum();
        DiscImporter.Progress callback=count->runOnUiThread(()->{
            progress=(int)Math.min(100,count*100/total);
            message=text("Импортировано ","Imported ")+(count/(1024*1024))+" / "+(total/(1024*1024))+text(" МБ"," MB")+" · "+progress+"%";pushState();
        });
        if(zip){
            try(InputStream input=getContentResolver().openInputStream(uri)){
                if(input==null)throw new IOException(text("Не удалось открыть ZIP","Could not open the ZIP"));
                DiscImporter.install(getFilesDir(),input,expected,callback);
            }
        }else{
            ParcelFileDescriptor descriptor=getContentResolver().openFileDescriptor(uri,"r");
            if(descriptor==null)throw new IOException(text("Не удалось открыть ISO","Could not open the ISO"));
            try(ParcelFileDescriptor.AutoCloseInputStream input=new ParcelFileDescriptor.AutoCloseInputStream(descriptor)){
                DiscImporter.installIso(getFilesDir(),input.getChannel(),expected,callback);
            }
        }
    }
}
