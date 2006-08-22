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
          <input class="formulaire" type='text' value='<TMPL_VAR NAME=dbi>' size='64' name='dbi'> 
         </td>
     </tr>
     <tr><td>user :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=user>' name='user'>
         </td>
     </tr>
     <tr><td>password :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=password>' name='password'> 
         </td></tr>

     <tr>  <td><b>General Options</b></td>  <td/></tr>

     <tr><td>email_media :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=email_media>' name='email_media'> 
         </td></tr>
         </td></tr>

     <tr>  <td><b>Bweb Configuration</b></td>  <td/></tr>

     <tr><td>graph_font :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=graph_font>' size='64' name='graph_font'> 
         </td></tr>
     <tr><td>template_dir :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=template_dir>' size='64' name='template_dir'> 
         </td></tr>
     <tr><td>bconsole :</td> 
         <td> <input class="formulaire" type='text' value='<TMPL_VAR NAME=bconsole>' size='64' name='bconsole'> 
         </td></tr>
     <tr><td>debug :</td> 
         <td> <input class="formulaire" type='checkbox' name='debug'> 
         </td></tr>
    </table>
    <button name='action' value='apply_conf' class='formulaire'>
     <img src='/bweb/save.png'>
    </button>
   </form>
</div>
