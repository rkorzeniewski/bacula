<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Configuration </h1>
</div>
<div class='bodydiv'>
   <table>
    <tr>  <td><b>SQL Connection</b></td>  <td/></tr>
    <tr><td>DBI :</td>      <td> <TMPL_VAR dbi>      </td></tr>
    <tr><td>user :</td>     <td> <TMPL_VAR user>     </td></tr>
    <tr><td>password :</td> <td> xxxxx </td></tr>
    <tr>  <td><b>General Options</b></td>  <td/></tr>
    <tr><td>email_media :</td> <td> <TMPL_VAR email_media> </td></tr>
    <tr>  <td><b>Bweb Configuration</b></td>  <td/></tr>
    <tr><td>config_file :</td> <td> <TMPL_VAR config_file> </td></tr>
    <tr><td title="/path/to/your/template_dir">template_dir :</td> <td> <TMPL_VAR template_dir> </td></tr>
    <tr><td title="/path/to/a/font.ttf">graph_font :</td> <td> <TMPL_VAR graph_font> </td></tr>
    <tr><td title="This folder must be writable by apache user and must be accessible on /bweb/fv">fv_write_path :</td> <td> <TMPL_VAR fv_write_path> </td></tr>
    <tr><td title="You can choose the Job table that you want to use to get statistics">stat_job_table :</td> <td> <TMPL_IF stat_job_table><TMPL_VAR stat_job_table><TMPL_ELSE>Job</TMPL_IF> </td></tr>
    <tr><td title="/path/to/bconsole -n -c /path/to/bconsole.conf">bconsole :</td> <td> <TMPL_VAR bconsole> </td></tr>
    <tr><td title="display timestamp in job log">display_log_time :</td> <td> <TMPL_VAR display_log_time> </td></tr>
    <tr><td title="user managment">security :</td> <td> <TMPL_VAR enable_security> </td></tr>
    <tr><td title="user filter">security acl :</td> <td> <TMPL_VAR enable_security_acl> </td></tr>
    <tr><td>debug :</td> <td> <TMPL_VAR debug> </td></tr>
    <TMPL_IF achs>
    <tr>  <td><b>Autochanger</b></td>  <td/></tr>
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
  
   <input type="image" name="action" value="ach_edit" title="edit" src='/bweb/edit.png'> 
   <input type="image" name='action' value='ach_del' title='delete' src='/bweb/remove.png'>
   <input type='image' name='action' value='ach_view' title='view' src='/bweb/zoom.png'>
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
   <input name='action' value='edit_conf' type="image" title='Edit' src='/bweb/edit.png'> Edit
   </label>
   <label>
   <input name='action' value='ach_add' type="image" title='Add an autochanger' src='/bweb/add.png'> Add autochanger
   </label>
  </form>

  <TMPL_IF error>
  info :  <TMPL_VAR error> </br>
  </TMPL_IF>
</div>
