package org.doaxbv.port;

import java.io.*;
import java.nio.channels.FileChannel;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/** Validated disc replacement; saves live beside the disc and are never moved. */
final class DiscImporter {
    static final class Expected {
        final long size; final String sha256;
        Expected(long size,String sha256){this.size=size;this.sha256=sha256;}
    }
    interface Progress { void bytes(long count); }
    private interface Extraction { void extract(Writer writer)throws Exception; }
    private static String hex(byte[] bytes){
        StringBuilder result=new StringBuilder();
        for(byte b:bytes)result.append(String.format(Locale.ROOT,"%02x",b&255));
        return result.toString();
    }
    static String hash(File file)throws Exception {
        MessageDigest digest=MessageDigest.getInstance("SHA-256");
        byte[] buffer=new byte[1024*1024];
        try(InputStream input=new FileInputStream(file)){
            int n;while((n=input.read(buffer))!=-1)digest.update(buffer,0,n);
        }
        return hex(digest.digest());
    }
    static void recover(File root)throws IOException {
        File disc=new File(root,"disc"),backup=new File(root,"disc.previous");
        if(!disc.exists()&&backup.exists()&&!backup.renameTo(disc))
            throw new IOException("Не удалось восстановить прежние игровые файлы");
    }
    static void install(File root,InputStream input,Map<String,Expected> expected,Progress progress)throws Exception {
        replace(root,expected,progress,writer->{
            try(ZipInputStream zip=new ZipInputStream(new BufferedInputStream(input))){
                ZipEntry entry;int count=0;
                while((entry=zip.getNextEntry())!=null){
                    if(++count>20000)throw new IOException("Слишком много файлов");
                    String name=entry.getName().replace('\\','/');
                    writer.destination(name);
                    if(!entry.isDirectory())writer.file(name,zip);
                }
            }
        });
    }
    static void installIso(File root,FileChannel channel,Map<String,Expected> expected,Progress progress)throws Exception {
        XboxIso iso=new XboxIso(channel);
        Map<String,XboxIso.Entry> files=iso.index();
        Map<String,String> canonical=new HashMap<>();
        for(String name:expected.keySet())canonical.put(name.toLowerCase(Locale.ROOT),name);
        Map<String,XboxIso.Entry> selected=new LinkedHashMap<>();
        for(Map.Entry<String,XboxIso.Entry> file:files.entrySet()){
            String name=canonical.get(file.getKey().toLowerCase(Locale.ROOT));
            if(name==null||selected.put(name,file.getValue())!=null)
                throw new IOException("ISO не соответствует официальной USA-версии этой сборки");
            if(file.getValue().size!=expected.get(name).size)
                throw new IOException("Неверный размер файла ISO: "+name);
        }
        if(!selected.keySet().equals(expected.keySet()))throw new IOException("В ISO не хватает игровых файлов");
        // Read files in physical order, which also works well on removable storage.
        List<String> names=new ArrayList<>(selected.keySet());
        Collections.sort(names,(a,b)->Long.compare(selected.get(a).offset,selected.get(b).offset));
        replace(root,expected,progress,writer->{
            for(String name:names)try(InputStream input=iso.open(selected.get(name))){writer.file(name,input);}
        });
    }
    private static final class Writer {
        final File staging;
        final String prefix;
        final Map<String,Expected> expected;
        final Set<String> found=new HashSet<>();
        final byte[] buffer=new byte[1024*1024];
        final Progress progress;
        long total,lastProgress;
        Writer(File staging,Map<String,Expected> expected,Progress progress)throws IOException{
            this.staging=staging;this.expected=expected;this.progress=progress;
            prefix=staging.getCanonicalPath()+File.separator;
        }
        File destination(String name)throws IOException{
            File destination=new File(staging,name);
            if(name.startsWith("/")||name.contains(":")||!destination.getCanonicalPath().startsWith(prefix))
                throw new IOException("Недопустимый путь в игровых файлах");
            return destination;
        }
        void file(String name,InputStream input)throws Exception{
            File destination=destination(name);
            Expected identity=expected.get(name);
            if(identity==null||!found.add(name))throw new IOException("Файлы не соответствуют официальной USA");
            File parent=destination.getParentFile();
            if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Не удалось создать папку");
            long size=0;MessageDigest digest=MessageDigest.getInstance("SHA-256");
            try(FileOutputStream output=new FileOutputStream(destination)){
                int n;while((n=input.read(buffer))!=-1){
                    if(n==0)throw new IOException("Не удалось прочитать игровые файлы");
                    size+=n;total+=n;
                    if(size>identity.size||total>8L*1024*1024*1024)throw new IOException("Неверный размер игровых файлов");
                    digest.update(buffer,0,n);output.write(buffer,0,n);
                    if(total-lastProgress>=32L*1024*1024){progress.bytes(total);lastProgress=total;}
                }
                output.getFD().sync();
            }
            if(size!=identity.size||!hex(digest.digest()).equals(identity.sha256))
                throw new IOException("Файл повреждён или отличается от официальной USA: "+name);
        }
    }
    private static void replace(File root,Map<String,Expected> expected,Progress progress,Extraction extraction)throws Exception {
        recover(root);
        File disc=new File(root,"disc"),staging=new File(root,"disc.importing"),backup=new File(root,"disc.previous");
        remove(staging);
        long required=64L*1024*1024;
        if(!expected.containsKey("default.xbe"))throw new IOException("Отсутствует описание версии игры");
        for(Expected file:expected.values()){
            if(file.size<0||file.size>8L*1024*1024*1024-required)throw new IOException("Неверное описание игровых файлов");
            required+=file.size;
        }
        if(root.getUsableSpace()<required)throw new IOException("Недостаточно свободного места: требуется ещё "+((required+1048575)/1048576)+" МБ");
        if(!staging.mkdirs())throw new IOException("Нет доступа к папке игры");
        boolean promoted=false;
        try {
            Writer writer=new Writer(staging,expected,progress);
            extraction.extract(writer);
            if(!writer.found.equals(expected.keySet()))throw new IOException("Не хватает игровых файлов");
            try(FileOutputStream marker=new FileOutputStream(new File(staging,".verified-usa"))){
                marker.write(expected.get("default.xbe").sha256.getBytes("UTF-8"));marker.getFD().sync();
            }
            remove(backup);
            if(disc.exists()&&!disc.renameTo(backup))throw new IOException("Не удалось подготовить замену игровых файлов");
            if(!staging.renameTo(disc)){
                recover(root);throw new IOException("Не удалось завершить импорт; прежняя игра восстановлена");
            }
            promoted=true;
            // A cleanup failure must not roll back or delete a successful import.
            try{remove(backup);}catch(IOException ignored){}
            progress.bytes(writer.total);
        }finally {
            if(!promoted)remove(staging);
        }
    }
    private static void remove(File file)throws IOException {
        if(file.isDirectory()){
            File[] children=file.listFiles();if(children==null)throw new IOException("Не удалось прочитать временную папку");
            for(File child:children)remove(child);
        }
        if(file.exists()&&!file.delete())throw new IOException("Не удалось удалить временный файл");
    }
}
