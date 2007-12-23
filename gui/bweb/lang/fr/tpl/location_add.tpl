<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Ajouter une localisation </h1>
</div>
<div class='bodydiv'>
   <form action="?" method='get'>
    <table>
     <tr><td>Localisation :</td>     
         <td> 
          <input class="formulaire" type='text' value='' size='32' name='location'> 
         </td>
     </tr>
     <tr><td>Coût :</td> 
         <td> <input class="formulaire" type='text' value='10' name='cost' size='3'>
         </td>
     </tr>
    <tr><td>En ligne :</td>
        <td> <select name='enabled' class='formulaire'>
           <option value='yes'>oui</option>
           <option value='no'>non</option>
           <option value='archived'>archivé</option>
           </select>
        </td>
    </tr>
    </table>
    <button type="submit" class="bp" name='action' value='location_add' title="Sauver" ><img src='/bweb/save.png' alt=''>Sauver</button>
   </form>

Tips: Vous devez avoir une localisation par robotique.

</div>
