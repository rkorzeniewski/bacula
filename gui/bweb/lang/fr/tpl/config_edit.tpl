<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Configuration </h1>
</div>
<div class='bodydiv'>
   <form action="?" method='post'>
    <table>
     <tr>  <td><b>Connexion SQL</b></td>  <td/></tr>
 
     <tr><td>DBI :</td>     
         <td> 
          <input class="formulaire" type='text' value='<TMPL_VAR NAME=dbi>' size='64' name='dbi'> 
         </td>
     </tr>
     <tr><td>user :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=user>' name='user'>
         </td>
     </tr>
     <tr><td>password :</td> 
         <td> <input class="formulaire" type='password' value='<TMPL_VAR NAME=password>' name='password'> 
         </td></tr>

     <tr>  <td><b>Options Générales</b></td>  <td/></tr>

     <tr><td>email_media :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=email_media>' name='email_media'> 
         </td></tr>
         </td></tr>

     <tr>  <td><b>Configuration Bweb</b></td>  <td/></tr>

     <tr><td>graph_font :</td> 
         <td> <input class="formulaire" title="/chemin/vers/une/font.ttf" type='text' value='<TMPL_VAR NAME=graph_font>' size='64' name='graph_font'> 
         </td></tr>
     <tr><td>template_dir :</td> 
         <td> <input class="formulaire" title="/chemin/vers/votre/template_dir" type='text' value='<TMPL_VAR NAME=template_dir>' size='64' name='template_dir'> 
         </td></tr>
     <tr><td>fv_write_path :</td> 
         <td> <input class="formulaire" title="Ce répertoire doit être accessible en écriture pour apache et être sous /bweb/fv" type='text' value='<TMPL_VAR fv_write_path>' size='64' name='fv_write_path'> 
         </td></tr>
     <tr><td>stat_job_table :</td> 
         <td> <input class="formulaire" title="Vous pouvez choisir une autre table que Job pour calculer vos statisques" type='text' value='<TMPL_IF stat_job_table><TMPL_VAR stat_job_table><TMPL_ELSE>Job</TMPL_IF>' size='64' name='stat_job_table'> 
         </td></tr>
     <tr><td>bconsole :</td> 
         <td> <input class="formulaire" title="/chemin/vers/bconsole -n -c /chemin/vers/bconsole.conf" type='text' value='<TMPL_VAR NAME=bconsole>' size='64' name='bconsole'> 
         </td></tr>
     <tr><td>display_log_time :</td>
         <td> <input class="formulaire" title="affiche les heures dans les logs" type='checkbox' name='display_log_time' <TMPL_IF display_log_time> checked='checked' value='on' </TMPL_IF> >
         </td></tr>
     <tr><td>debug :</td> 
         <td> <input class="formulaire" type='checkbox' name='debug' <TMPL_IF debug> checked='checked' value='on' </TMPL_IF> > 
         </td></tr>
    </table>
    <input type="image" name='action' value='apply_conf' src='/bweb/save.png'>
   </form>
</div>
