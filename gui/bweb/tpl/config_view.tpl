<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Configuration </h1>
</div>
<div class='bodydiv'>
   <table>
    <tr>  <td><b>SQL Connection</b></td>  <td/></tr>
    <tr><td>DBI :</td>      <td> <TMPL_VAR dbi>      </td></tr>
    <tr><td>user :</td>     <td> <TMPL_VAR user>     </td></tr>
    <tr><td>password :</td> <td> <TMPL_VAR password> </td></tr>
    <tr>  <td><b>General Options</b></td>  <td/></tr>
    <tr><td>email_media :</td> <td> <TMPL_VAR email_media> </td></tr>
    <tr>  <td><b>Bweb Configuration</b></td>  <td/></tr>
    <tr><td>template_dir :</td> <td> <TMPL_VAR template_dir> </td></tr>
    <tr><td>graph_font :</td> <td> <TMPL_VAR graph_font> </td></tr>
    <tr><td>bconsole :</td> <td> <TMPL_VAR bconsole> </td></tr>
    <tr><td>debug :</td> <td> <TMPL_VAR debug> </td></tr>
    <TMPL_IF achs>
    <tr>  <td><b>Autochanger</b></td>  <td/></tr>
    <tr>
     <td>
     <form action='?' method='GET'>
     <table border='0'>
    <TMPL_LOOP achs>
      <tr> 
       <td><input type='radio' name='ach' value='<TMPL_VAR name>'><TMPL_VAR name></td>
      </tr>
    </TMPL_LOOP>
    </table>
   <td>
    <button class='formulaire' type='submit' name='action' value='ach_edit' title='edit'><img src='/bweb/edit.png'></button>
    <button class='formulaire' type='submit' name='action' value='ach_del' title='delete'><img src='/bweb/remove.png'></button>
    <button class='formulaire' type='submit' name='action' value='ach_view' title='view'><img src='/bweb/zoom.png'></button>
    </form>
    </td>
   </tr>
   </TMPL_IF achs>
  </table>

  <form action='?' method='GET'>
   <button name='action' value='edit_conf' class='formulaire'>
      Edit<br/><img title='Edit' src='/bweb/edit.png'>
   </button>
   <button name='action' value='ach_add' class='formulaire'>
      Add autochanger<br/><img title='Add an autochanger' src='/bweb/add.png'>
   </button>
  </form>

  info :  <TMPL_VAR error> </br>

</div>
