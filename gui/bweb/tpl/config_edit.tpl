<br/>
<div class='titlediv'>
  <h1 class='newstitle'> __Configuration__ </h1>
</div>
<div class='bodydiv'>
   <form action="?" method='post'>
    <table>
     <tr>  <td><b>__SQL Connection__</b></td>  <td/></tr>
 
     <tr><td>__DBI:__</td>     
         <td> 
          <input class="formulaire" 
title='DBI:Pg:database=bacula;host=yourhost  or  DBI:Mysql:database=bacula   or...' 
                 type='text' value='<TMPL_VAR dbi>' size='64' name='dbi'> 
         </td>
     </tr>
     <tr><td>__user:__</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR user>' name='user'>
         </td>
     </tr>
     <tr><td>__password:__</td> 
         <td> <input class="formulaire" type='password' value='<TMPL_VAR password>' name='password'> 
         </td></tr>

     <tr>  <td><b>__General Options__</b></td>  <td/></tr>

     <tr><td>email_media:</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR email_media>' name='email_media'> 
         </td></tr>
         </td></tr>

     <tr>  <td><b>__Bweb Configuration__</b></td>  <td/></tr>

     <tr><td>graph_font:</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR graph_font>' size='64' name='graph_font'> 
         </td></tr>
     <tr><td>fv_write_path:</td> 
         <td> <input class="formulaire" title="__This folder must be writable by apache user and must be accessible on /bweb/fv__" type='text' value='<TMPL_VAR fv_write_path>' size='64' name='fv_write_path'> 
     <tr><td>stat_job_table:</td> 
         <td> <input class="formulaire" title="__You can choose the Job table that you want to use to get statistics__" type='text' value='<TMPL_IF stat_job_table><TMPL_VAR stat_job_table><TMPL_ELSE>Job</TMPL_IF>' size='64' name='stat_job_table'> 
         </td></tr>
     <tr><td>bconsole:</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR bconsole>' size='64' name='bconsole'> 
         </td></tr>
     <tr><td>wiki_url:</td> 
         <td> <input class="formulaire" title="__Use a wiki for jobs documentation?__" size='64' type='text' name='wiki_url' value='<TMPL_VAR wiki_url>'> 
         </td></tr>
     <tr><td>template_dir:</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR template_dir>' size='64' name='template_dir'> 
         </td></tr>
     <tr><td>__language:__</td> 
     <td> <select name="lang" id='lang' class="formulaire">
           <option id='lang_en' value='en'>__English__</option>
           <option id='lang_fr' value='fr'>__French__</option>
           <option id='lang_es' value='es'>__Spanish__</option>
          </select>
     </td></tr>
     <tr><td>__default_age:__</td>
         <td> <input class="formulaire" type='text' value='<TMPL_VAR default_age>' title='24h15m' size='64' name='default_age'> 
         </td></tr>
     <tr><td>display_log_time:</td> 
         <td> <input class="formulaire" title="__Display log timestamp__" type='checkbox' name='display_log_time' <TMPL_IF display_log_time> checked='checked' value='on' </TMPL_IF> > 
         </td></tr>
     <tr><td>security:</td> 
         <td> <input class="formulaire" type='checkbox' name='enable_security' title='__Use user managment in bweb. Read INSTALL first__' <TMPL_IF enable_security> checked='checked' value='on' </TMPL_IF> > 
     <tr><td>security acl:</td> 
         <td> <input class="formulaire" type='checkbox' name='enable_security_acl' title='__Use user acl in bweb. Read INSTALL first__' <TMPL_IF enable_security_acl> checked='checked' value='on' </TMPL_IF> > 
     <tr><td>debug:</td> 
         <td> <input class="formulaire" type='checkbox' name='debug' <TMPL_IF debug> checked='checked' value='on' </TMPL_IF> > 
         </td></tr>
    </table>
    <button type="submit" class="bp" name='action' value='apply_conf'> <img src='/bweb/save.png' alt=''>__Save__</button>
   </form>
</div>

<script type="text/javascript" language='JavaScript'>
<TMPL_IF lang>
  document.getElementById('lang_<TMPL_VAR lang>').selected = true;
</TMPL_IF>
</script>