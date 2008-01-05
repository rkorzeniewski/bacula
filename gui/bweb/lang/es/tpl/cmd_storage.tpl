
<div class='titlediv'>
  <h1 class='newstitle'> Manage Storage device</h1>
</div>
<div class='bodydiv'>
   <form action="?" method='get'>
   <input type='hidden' name='action' value='cmd_storage'>
    <table>
     <tr><td>Storage device:</td>
         <td> 
            <select name='storage' size='5' class='formulaire'>
            <TMPL_LOOP storage>
            <option><TMPL_VAR name></option>
            </TMPL_LOOP>
            </select>
         </td>
     </tr>
     <tr><td>Drive number (if any):</td> 
         <td><input size='3' type='text' name='drive' value='0' class='formulaire'></td>
     </tr>
     <tr><td>Action:</td> 
         <td>
<button type="submit" class="bp" name='storage_cmd' value='mount'
 title='mount drive'> <img src='/bweb/load.png' alt=''>Mount</button>
<button type="submit" class="bp" name='storage_cmd' value='umount'
 title='umount drive'> <img src='/bweb/unload.png' alt=''>Umount</button>
<button type="submit" class="bp" name='storage_cmd' value='release'
 title='release drive'> <img src='/bweb/unload.png' alt=''>Release</button>
<button type="submit" class="bp" name='storage_cmd' value='status'
 title='status drive'> <img src='/bweb/zoom.png' alt=''>Estado</button>
         </td>
     </tr>
    </table>
   </form>
</div>

