
<div class='titlediv'>
  <h1 class='newstitle'> __Manage Storage devices__</h1>
</div>
<div class='bodydiv'>
   <form action="?" method='get'>
   <input type='hidden' name='action' value='cmd_storage'>
    <table>
     <tr><td>__Storage device:__</td>
         <td> 
            <select name='storage' size='5' class='formulaire'>
            <TMPL_LOOP storage>
            <option><TMPL_VAR name></option>
            </TMPL_LOOP>
            </select>
         </td>
     </tr>
     <tr><td>__Drive number (if any):__</td> 
         <td><input size='3' type='text' name='drive' value='0' class='formulaire'></td>
     </tr>
     <tr><td>__Action:__</td> 
         <td>
<button type="submit" class="bp" name='storage_cmd' value='mount'
 title='__mount drive__'> <img src='/bweb/load.png' alt=''>__Mount__</button>
<button type="submit" class="bp" name='storage_cmd' value='umount'
 title='__umount drive__'> <img src='/bweb/unload.png' alt=''>__Umount__</button>
<button type="submit" class="bp" name='storage_cmd' value='release'
 title='__release drive__'> <img src='/bweb/unload.png' alt=''>__Release__</button>
<button type="submit" class="bp" name='storage_cmd' value='status'
 title='__status drive__'> <img src='/bweb/zoom.png' alt=''>__Status__</button>
         </td>
     </tr>
    </table>
   </form>
</div>

