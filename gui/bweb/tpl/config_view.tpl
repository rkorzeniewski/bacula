<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Configuration </h1>
</div>
<div class='bodydiv'>
   <table>
    <tr class='header'>  <td>SQL connection </td>  <td/></tr>
    <tr>  <td><b>SQL Connection</b></td>  <td/></tr>
    <tr><td>DBI :</td>      <td> <TMPL_VAR NAME=dbi>      </td></tr>
    <tr><td>user :</td>     <td> <TMPL_VAR NAME=user>     </td></tr>
    <tr><td>password :</td> <td> <TMPL_VAR NAME=password> </td></tr>
    <tr>  <td><b>General Options</b></td>  <td/></tr>
    <tr><td>email_media :</td> <td> <TMPL_VAR NAME=email_media> </td></tr>
    <tr>  <td><b>Bweb Configuration</b></td>  <td/></tr>
    <tr><td>template_dir :</td> <td> <TMPL_VAR NAME=template_dir> </td></tr>
    <tr><td>graph_font :</td> <td> <TMPL_VAR NAME=graph_font> </td></tr>
    <tr><td>bconsole :</td> <td> <TMPL_VAR NAME=bconsole> </td></tr>
    <tr><td>debug :</td> <td> <TMPL_VAR NAME=debug> </td></tr>
   </table>

  info :  <TMPL_VAR NAME=error> </br>

  <form action='?' method='GET'>
   <button name='action' value='edit_conf' class='formulaire'>
      <img title='Edit' src='/bweb/edit.png'>
   </button>
  </form>

</div>
