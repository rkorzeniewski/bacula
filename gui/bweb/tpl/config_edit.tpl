<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Configuration </h1>
</div>
<div class='bodydiv'>
   <form action="?" method='post'>
    <table>
     <tr>  <td><b>SQL Connection</b></td>  <td/></tr>
 
     <tr><td>DBI :</td>     
         <td> 
          <input class="formulaire" type='text' value='<TMPL_VAR dbi>' size='64' name='dbi'> 
         </td>
     </tr>
     <tr><td>user :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR user>' name='user'>
         </td>
     </tr>
     <tr><td>password :</td> 
         <td> <input class="formulaire" type='password' value='<TMPL_VAR password>' name='password'> 
         </td></tr>

     <tr>  <td><b>General Options</b></td>  <td/></tr>

     <tr><td>email_media :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR email_media>' name='email_media'> 
         </td></tr>
         </td></tr>

     <tr>  <td><b>Bweb Configuration</b></td>  <td/></tr>

     <tr><td>graph_font :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR graph_font>' size='64' name='graph_font'> 
         </td></tr>
     <tr><td>template_dir :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR template_dir>' size='64' name='template_dir'> 
         </td></tr>
     <tr><td>fv_write_path :</td> 
         <td> <input class="formulaire" title="This folder must be writable by apache user and must be accessible on /bweb/fv" type='text' value='<TMPL_VAR fv_write_path>' size='64' name='fv_write_path'> 
         </td></tr>
     <tr><td>bconsole :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR bconsole>' size='64' name='bconsole'> 
         </td></tr>
     <tr><td>debug :</td> 
         <td> <input class="formulaire" type='checkbox' name='debug' <TMPL_IF debug> checked='checked' value='on' </TMPL_IF> > 
         </td></tr>
    </table>
    <input type="image" name='action' value='apply_conf' src='/bweb/save.png'>
   </form>
</div>
