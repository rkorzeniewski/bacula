<br/>
<div class='titlediv'>
  <h1 class='newstitle'> New autochanger </h1>
</div>
<div class='bodydiv'>
   <form action="?" method='get'>
    <table>
     <tr><td>Name :</td>     
         <td>
          <select name='ach' class='formulaire'>
<TMPL_LOOP devices><option value='<TMPL_VAR name>'><TMPL_VAR name></option></TMPL_LOOP>
          </select>
         </td>
     </tr>
     <tr><td>Pre-command :</td> 
         <td> <input class="formulaire" type='text' value='sudo' title='can be "sudo" or "ssh storage@storagehost"...' name='precmd'>
         </td>
     </tr>
     <tr><td>mtx command :</td> 
         <td> <input class="formulaire" type='text' value='/usr/sbin/mtx' name='mtxcmd' size='32'>
         </td>
     </tr>
     <tr><td>Device :</td> 
         <td> <input class="formulaire" type='text' value='/dev/changer' name='device'>
         </td>
     </tr>
    <tr><td><b>Drives</b></td><td/></tr>
    <TMPL_LOOP devices>
    <tr>
     <td><input class='formulaire' type='checkbox' 
                name='drive' value='<TMPL_VAR name>'><TMPL_VAR name>
     </td>
     <td>index <input type='text' title='drive index' class='formulaire'
                value='' name='index_<TMPL_VAR name>' size='3'>
     </td>
    </tr>
    </TMPL_LOOP>
    </table>
    <button name='action' value='ach_add' class='formulaire'>
     <img src='/bweb/save.png'>
    </button>
   </form>
</div>
