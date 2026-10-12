package com.analoglav.amps.usb;

import android.content.Context;
import android.util.AtomicFile;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/**
 * Durable diagnostic snapshot in private app storage.
 * Android's document picker can recreate Activity before returning a URI;
 * therefore the report must not exist only in an Activity field.
 */
public final class UsbDiagnosticStore {
  private static final String FILE_NAME="amps_usb_diagnostic_latest.txt";
  private UsbDiagnosticStore() {}

  private static AtomicFile file(Context context) {
    File target=new File(context.getFilesDir(),FILE_NAME);
    return new AtomicFile(target);
  }

  public static void save(Context context,String report) throws IOException {
    String content=report==null || report.isEmpty() ?
        "AMPS DIAGNOSTIC ERROR: empty source report\n" : report;
    AtomicFile target=file(context);
    FileOutputStream out=target.startWrite();
    try {
      out.write(content.getBytes(StandardCharsets.UTF_8));
      out.flush();
      target.finishWrite(out);
    }catch(IOException | RuntimeException e) {
      target.failWrite(out);
      throw e;
    }
  }

  public static String load(Context context) throws IOException {
    AtomicFile target=file(context);
    try(FileInputStream in=target.openRead();
        ByteArrayOutputStream out=new ByteArrayOutputStream()) {
      byte[] buffer=new byte[4096];
      int n;
      while((n=in.read(buffer))!=-1) {
        out.write(buffer,0,n);
        if(out.size()>262144)throw new IOException("diagnostic exceeds size limit");
      }
      String content=out.toString(StandardCharsets.UTF_8.name());
      if(content.isEmpty())throw new IOException("diagnostic snapshot empty");
      return content;
    }
  }
}
