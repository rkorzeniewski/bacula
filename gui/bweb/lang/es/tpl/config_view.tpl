<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Configuración </h1>
</div>
<div class='bodydiv'>
   <table>
    <tr>  <td><b>Conexión SQL</b></td>  <td/></tr>
    <tr><td>DBI :</td>      <td> <TMPL_VAR dbi>      </td></tr>
    <tr><td>Ususario :</td>     <td> <TMPL_VAR user>     </td></tr>
    <tr><td>Clave :</td> <td> xxxxx </td></tr>
    <tr>  <td><b>Opciones Generales</b></td>  <td/></tr>
    <tr><td>email_media :</td> <td> <TMPL_VAR email_media> </td></tr>
    <tr>  <td><b>Configuración Bweb</b></td>  <td/></tr>
    <tr><td>config_file :</td> <td> <TMPL_VAR config_file> </td></tr>
    <tr><td title="/path/a/tu/template_dir">template_dir :</td> <td> <TMPL_VAR template_dir> </td></tr>
    <tr><td title="/path/a/a/font.ttf">graph_font :</td> <td> <TMPL_VAR graph_font> </td></tr>
    <tr><td title="Este directorio debe tener permisos de escritura para el usuario apache y debe ser accesible en /bweb/fv">fv_write_path :</td> <td> <TMPL_VAR fv_write_path> </td></tr>
    <tr><td title="You can choose the Job table that you want to use to get statistics">stat_job_table :</td> <td> <TMPL_IF stat_job_table><TMPL_VAR stat_job_table><TMPL_ELSE>Job</TMPL_IF> </td></tr>
    <tr><td title="/path/a/bconsole -n -c /path/to/bconsole.conf">bconsole :</td> <td> <TMPL_VAR bconsole> </td></tr>
    <tr><td title="display timestamp in job log">display_log_time :</td> <td> <TMPL_VAR display_log_time> </td></tr>
    <tr><td>debug :</td> <td> <TMPL_VAR debug> </td></tr>
    <TMPL_IF achs>
    <tr>  <td><b>Libreria</b></td>  <td/></tr>
    <tr>
     <td>
     <form action='?' method='GET'>
     <table border='0'>
    <TMPL_LOOP achs>
      <tr> 
       <td>
<label>
 <input type='radio' name='ach' value='<TMPL_VAR name>'><TMPL_VAR name>
</label>
       </td>
      </tr>
    </TMPL_LOOP>
    </table>
   <td>
  
   <input type="image" name="action" value="ach_edit" title="editar" src='/bweb/edit.png'> 
   <input type="image" name='action' value='ach_del' title='borrar' src='/bweb/remove.png'>
   <input type='image' name='action' value='ach_view' title='ver' src='/bweb/zoom.png'>
    </form>
    </td>
   </tr>
   </TMPL_IF achs>
   <tr>
   <td><hr></td><td></td>
   </tr>
  </table>

  <form action='?' method='GET'>
   <label>
   <input name='action' value='edit_conf' type="image" title='Edit' src='/bweb/edit.png'> Editar
   </label>
   <label>
   <input name='action' value='ach_add' type="image" title='Add an autochanger' src='/bweb/add.png'> Agregar libreria
   </label>
  </form>

  <TMPL_IF error>
  info :  <TMPL_VAR error> </br>
  </TMPL_IF>
</div>
