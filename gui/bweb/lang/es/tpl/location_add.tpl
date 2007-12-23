<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Nueva Ubicación </h1>
</div>
<div class='bodydiv'>
   <form action="?" method='get'>
    <table>
     <tr><td>Ubicación :</td>     
         <td> 
          <input class="formulaire" type='text' value='' size='32' name='location'> 
         </td>
     </tr>
     <tr><td>Cost :</td> 
         <td> <input class="formulaire" type='text' value='10' name='cost' size='3'>
         </td>
     </tr>
    <tr><td>Activado :</td>
        <td> <select name='enabled' class='formulaire'>
           <option value='yes'>yes</option>
           <option value='no'>no</option>
           <option value='archived'>archived</option>
           </select>
        </td>
    </tr>
    </table>
    <button type="submit" class="bp" name='action' value='location_add' title="Save" ><img src='/bweb/save.png' alt=''>Save</button>
   </form>

Tips: It's a good idea to have a location per autochanger.

</div>
