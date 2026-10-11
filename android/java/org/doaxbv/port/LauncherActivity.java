package org.doaxbv.port;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.content.SharedPreferences;
import android.widget.*;
import java.io.*;
import java.util.*;
import org.json.JSONObject;

public class LauncherActivity extends Activity {
    private TextView status;
    private Button play,importButton;
    private CheckBox swapButtons;
    private Spinner touchControls,resolution,displayAspect,language;
    private SharedPreferences preferences;
    private boolean english;
    private volatile boolean importing;
    private int importOrientation;
    private static final String XBE_HASH="053d44e885fa33c1d15d909a533f39dfbd976e97eeaf67e4fdef8438ea7e5c54";
    File disc(){return new File(getFilesDir(),"disc");}
    private String text(String ru,String en){return english ? en : ru;}
    static String savedAspect(SharedPreferences preferences){
        String value=preferences.getString("displayAspect",null);
        if("4:3".equals(value)||"16:9".equals(value)||"source".equals(value))return value;
        return preferences.getBoolean("aspectCorrected",false)&&preferences.getBoolean("display43",false) ? "4:3" : "source";
    }
    private void addChoice(LinearLayout layout,String title,Spinner choice){
        LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(android.view.Gravity.CENTER_VERTICAL);
        TextView label=new TextView(this);label.setText(title);label.setTextSize(16);row.addView(label);
        row.addView(choice,new LinearLayout.LayoutParams(0,LinearLayout.LayoutParams.WRAP_CONTENT,1));layout.addView(row);
    }
    private boolean saveOptions(){
        String aspect=new String[]{"source","4:3","16:9"}[displayAspect.getSelectedItemPosition()];
        return preferences.edit().putString("displayAspect",aspect).putString("language",language.getSelectedItemPosition()==1 ? "en" : "ru")
            .putInt("resolution",resolution.getSelectedItemPosition()).putBoolean("swapButtons",swapButtons.isChecked())
            .putInt("touchControls",touchControls.getSelectedItemPosition()).commit();
    }
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        preferences=getSharedPreferences("controls",MODE_PRIVATE);
        english="en".equals(preferences.getString("language","ru"));
        LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);int pad=(int)(20*getResources().getDisplayMetrics().density);layout.setPadding(pad,pad,pad,pad);
        TextView title=new TextView(this);title.setText("Dead or Alive\nXtreme Beach Volleyball");title.setTextSize(26);layout.addView(title);
        status=new TextView(this);status.setPadding(0,pad,0,pad);layout.addView(status);
        displayAspect=new Spinner(this);displayAspect.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{text("Исходные пропорции","Original proportions"),"4:3","16:9"}));
        String aspect=savedAspect(preferences);displayAspect.setSelection("4:3".equals(aspect)?1:"16:9".equals(aspect)?2:0);addChoice(layout,text("Формат","Aspect ratio"),displayAspect);
        language=new Spinner(this);language.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{"Русский","English"}));language.setSelection(english?1:0);addChoice(layout,text("Язык","Language"),language);
        swapButtons=new CheckBox(this);swapButtons.setText(text("Поменять A/B и X/Y на геймпаде","Swap A/B and X/Y on the gamepad"));swapButtons.setChecked(preferences.getBoolean("swapButtons",false));layout.addView(swapButtons);
        TextView touchLabel=new TextView(this);touchLabel.setText(text("Экранные кнопки","Touch controls"));layout.addView(touchLabel);
        touchControls=new Spinner(this);touchControls.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{text("Авто — скрывать при подключённом геймпаде","Auto — hide when a gamepad is connected"),text("Показывать","Show"),text("Скрывать","Hide")}));touchControls.setSelection(preferences.getInt("touchControls",0));layout.addView(touchControls);
        TextView resolutionLabel=new TextView(this);resolutionLabel.setText(text("Внутреннее разрешение","Internal resolution"));layout.addView(resolutionLabel);
        resolution=new Spinner(this);resolution.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{text("Обычное — 720 × 480","Standard — 720 × 480"),text("Повышенное — 1440 × 960","Higher — 1440 × 960")}));resolution.setSelection(preferences.getInt("resolution",0));layout.addView(resolution);
        play=new Button(this);play.setText(text("Играть","Play"));layout.addView(play);play.setOnClickListener(v->{if(saveOptions())startActivity(new Intent(this,GameActivity.class));else status.setText(text("Не удалось сохранить настройки.","Could not save settings."));});
        importButton=new Button(this);importButton.setText(text("Импортировать файлы USA из ISO","Import USA files from ISO"));layout.addView(importButton);
        importButton.setOnClickListener(v->{Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.setType("*/*");intent.addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(intent,1);});
        TextView help=new TextView(this);help.setText(text("Формат и язык применяются при запуске игры.\nL1 — Black, R1 — White, L2/R2 — триггеры Xbox.\nЕсли буквы кнопок не совпадают, включите перестановку A/B и X/Y.\nСохранения хранятся в приложении.\nВыберите ISO оригинальной USA-версии игры во внутренней памяти или на карте exFAT. ZIP этой сборки также поддерживается. Во время импорта оставьте приложение открытым.","Aspect ratio and language apply when the game starts.\nL1 — Black, R1 — White, L2/R2 — Xbox triggers.\nIf the button labels differ, enable A/B and X/Y swapping.\nSaves are stored in the application.\nSelect an ISO of the original USA game from local storage or an exFAT card. This build also accepts its game-data ZIP. Keep the app open while importing."));layout.addView(help);ScrollView scroll=new ScrollView(this);scroll.addView(layout);setContentView(scroll);refresh();play.requestFocus();
        language.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){
            public void onNothingSelected(AdapterView<?> parent){}
            public void onItemSelected(AdapterView<?> parent,android.view.View view,int position,long id){
                String selected=position==1 ? "en" : "ru";
                if(!selected.equals(preferences.getString("language","ru"))&&saveOptions())recreate();
            }
        });
    }
    @Override protected void onResume(){super.onResume();if(status!=null&&!importing)refresh();}
    private void refresh(){
        boolean ready=false;
        try{DiscImporter.recover(getFilesDir());ready=new File(disc(),".verified-usa").isFile()
            &&XBE_HASH.equals(DiscImporter.hash(new File(disc(),"default.xbe")))
            &&new File(disc(),"datadvd.afs").isFile();}catch(Exception ignored){}
        play.setEnabled(ready&&!importing);
        status.setText(ready ? text("Официальные ресурсы USA установлены.\nИсходные голоса.","Official USA resources installed.\nOriginal voices.")
            : text("Импортируйте ISO оригинальной USA-версии игры.\nСтарые игровые файлы будут заменены; сохранения останутся.","Import an ISO of the original USA game.\nExisting game files will be replaced; saves will be retained."));
    }
    @Override public void onBackPressed(){
        if(importing)Toast.makeText(this,text("Дождитесь завершения импорта","Wait for the import to finish"),Toast.LENGTH_SHORT).show();
        else super.onBackPressed();
    }
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request!=1||result!=RESULT_OK||data==null||importing)return;Uri uri=data.getData();if(uri==null)return;importing=true;play.setEnabled(false);importButton.setEnabled(false);language.setEnabled(false);
        importOrientation=getRequestedOrientation();setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        status.setText(text("Чтение образа и проверка игровых файлов…","Reading the image and checking game files…"));
        new Thread(()->{String resultText;try{importGame(uri);resultText=text("Импорт завершён.","Import complete.");}catch(Exception error){resultText=text("Не удалось импортировать: ","Import failed: ")+error.getMessage();}final String message=resultText;runOnUiThread(()->{importing=false;getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);importButton.setEnabled(true);language.setEnabled(true);refresh();status.setText(message);setRequestedOrientation(importOrientation);});},"doaxbv-import").start();}
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
        DiscImporter.Progress progress=count->runOnUiThread(()->status.setText(text("Импортировано ","Imported ")+(count/(1024*1024))+" / "+(total/(1024*1024))+text(" МБ"," MB")+" ("+(count*100/total)+"%)"));
        if(zip){
            try(InputStream input=getContentResolver().openInputStream(uri)){
                if(input==null)throw new IOException(text("Не удалось открыть ZIP","Could not open the ZIP"));
                DiscImporter.install(getFilesDir(),input,expected,progress);
            }
        }else{
            ParcelFileDescriptor descriptor=getContentResolver().openFileDescriptor(uri,"r");
            if(descriptor==null)throw new IOException(text("Не удалось открыть ISO","Could not open the ISO"));
            try(ParcelFileDescriptor.AutoCloseInputStream input=new ParcelFileDescriptor.AutoCloseInputStream(descriptor)){
                DiscImporter.installIso(getFilesDir(),input.getChannel(),expected,progress);
            }
        }
    }
}
