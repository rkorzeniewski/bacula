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
  
   <button type="submit" class="bp" name="action" value="ach_edit" title="edit"> <img src='/bweb/edit.png' alt=''>Edit</button>
   <button type="submit" class="bp" name='action' value='ach_del' title='delete'> <img src='/bweb/remove.png' alt=''>Delete</button>
   <button type="submit" class="bp" name='action' value='ach_view' title='view'> <img src='/bweb/zoom.png' alt=''>View</button>
    </form>
    </td>
   </tr>
   </TMPL_IF achs>
   <tr>
   <td><hr></td><td></td>
   </tr>
  </table>

  <form action='?' method='GET'>
   <button name='action' value='edit_conf' type="submit" class="bp" title='Edit'> <img src='/bweb/edit.png' alt=''>Edit</button>
   <button name='action' value='ach_add' type="submit" class="bp" title='Add an autochanger'> <img src='/bweb/add.png' alt=''>Add autochanger</button>
  </form>

  <TMPL_IF error>
  info :  <TMPL_VAR error> </br>
  </TMPL_IF>
</div>
