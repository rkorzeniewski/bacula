<br/>
<div class='titlediv'>
  <h1 class='newstitle'> __Configuration__ </h1>
</div>
<div class='bodydiv'>
   <table>
    <tr>  <td><b>__SQL Connection__</b></td>  <td/></tr>
    <tr><td>__DBI:__</td>      <td> <TMPL_VAR dbi>      </td></tr>
    <tr><td>__user:__</td>     <td> <TMPL_VAR user>     </td></tr>
    <tr><td>__password:__</td> <td> xxxxx </td></tr>
    <tr>  <td><b>__General Options__</b></td>  <td/></tr>
    <tr><td>email_media:</td> <td> <TMPL_VAR email_media> </td></tr>
    <tr>  <td><b>__Bweb Configuration__</b></td>  <td/></tr>
    <tr><td>config_file:</td> <td> <TMPL_VAR config_file> </td></tr>
    <tr><td title="/path/to/your/template_dir">template_dir:</td> <td> <TMPL_VAR template_dir> </td></tr>
    <tr><td title="/path/to/a/font.ttf">graph_font:</td> <td> <TMPL_VAR graph_font> </td></tr>
    <tr><td title="__This folder must be writable by apache user and must be accessible on /bweb/fv__">fv_write_path:</td> <td> <TMPL_VAR fv_write_path> </td></tr>
    <tr><td title="__You can choose the Job table that you want to use to get statistics__">stat_job_table:</td> <td> <TMPL_IF stat_job_table><TMPL_VAR stat_job_table><TMPL_ELSE>Job</TMPL_IF> </td></tr>
    <tr><td title="/path/to/bconsole -n -c /path/to/bconsole.conf">bconsole:</td> <td> <TMPL_VAR bconsole> </td></tr>
    <tr><td title="__use a wiki for jobs documentation?__">wiki_url:</td> <td> <TMPL_VAR wiki_url> </td></tr>
    <tr><td title="__display timestamp in job log__">display_log_time:</td> <td> <TMPL_VAR display_log_time> </td></tr>
    <tr><td title="__user managment__">__security:__</td> <td> <TMPL_VAR enable_security> </td></tr>
    <tr><td title="__user filter__">__security acl:__</td> <td> <TMPL_VAR enable_security_acl> </td></tr>
    <tr><td>__debug:__</td> <td> <TMPL_VAR debug> </td></tr>
    <TMPL_IF achs>
    <tr>  <td><b>__Autochanger__</b></td>  <td/></tr>
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
  
   <button type="submit" class="bp" name="action" value='ach_edit' title="__Edit__"> <img src='/bweb/edit.png' alt=''>__Edit__</button>
   <button type="submit" class="bp" name='action' value='ach_del' title='__Delete__'> <img src='/bweb/remove.png' alt=''>__Delete__</button>
   <button type="submit" class="bp" name='action' value='ach_view' title='__View__'> <img src='/bweb/zoom.png' alt=''>__View__</button>
    </form>
    </td>
   </tr>
   </TMPL_IF achs>
   <tr>
   <td><hr></td><td></td>
   </tr>
  </table>

  <form action='?' method='GET'>
   <button name='action' value='edit_conf' type="submit" class="bp" title='__Edit__'> <img src='/bweb/edit.png' alt=''>__Edit__</button>
   <button name='action' value='ach_add' type="submit" class="bp" title='__Add autochanger__'> <img src='/bweb/add.png' alt=''>__Add autochanger__</button>
  </form>

  <TMPL_IF error>
  __info:__  <TMPL_VAR error> </br>
  </TMPL_IF>
</div>
